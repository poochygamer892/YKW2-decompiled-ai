#include <jni.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include "ykw2/core/import_contract.hpp"
#include "ykw2/core/romfs_extract.hpp"

namespace {

bool read_header(int file_descriptor, std::array<std::uint8_t, 0x200>& header) {
    std::size_t read_bytes = 0;
    while (read_bytes < header.size()) {
        const auto result = pread(file_descriptor, header.data() + read_bytes,
                                  header.size() - read_bytes,
                                  static_cast<off_t>(read_bytes));
        if (result <= 0) {
            return false;
        }
        read_bytes += static_cast<std::size_t>(result);
    }
    return true;
}

bool ensure_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    for (std::size_t index = 1; index <= path.size(); ++index) {
        if (index != path.size() && path[index] != '/') {
            continue;
        }
        const auto directory = path.substr(0, index);
        if (directory.empty()) {
            continue;
        }
        if (mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

class FdSource final : public ykw2::core::ResourceSource {
public:
    FdSource(int file_descriptor, std::uint64_t image_size)
        : file_descriptor_(file_descriptor), image_size_(image_size) {}

    std::uint64_t size() const override { return image_size_; }

    bool read(std::uint64_t offset, std::uint8_t* destination,
              std::size_t length) const override {
        if (!contains(offset, length)) {
            return false;
        }
        std::size_t read_bytes = 0;
        while (read_bytes < length) {
            const auto result = pread(file_descriptor_, destination + read_bytes,
                                      length - read_bytes,
                                      static_cast<off_t>(offset + read_bytes));
            if (result <= 0) {
                return false;
            }
            read_bytes += static_cast<std::size_t>(result);
        }
        return true;
    }

private:
    int file_descriptor_;
    std::uint64_t image_size_;
};

class DirectorySink final : public ykw2::core::ResourceSink {
public:
    explicit DirectorySink(std::string root) : root_(std::move(root)) {}

    bool begin_file(std::string_view path, std::uint64_t) override {
        if (file_descriptor_ >= 0) {
            return false;
        }
        current_path_ = root_ + "/" + std::string(path);
        const auto separator = current_path_.find_last_of('/');
        if (separator == std::string::npos || !ensure_directory(current_path_.substr(0, separator))) {
            current_path_.clear();
            return false;
        }
        file_descriptor_ = open(current_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        return file_descriptor_ >= 0;
    }

    bool write(const std::uint8_t* data, std::size_t size) override {
        if (file_descriptor_ < 0) {
            return false;
        }
        std::size_t written = 0;
        while (written < size) {
            const auto result = ::write(file_descriptor_, data + written, size - written);
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result <= 0) {
                return false;
            }
            written += static_cast<std::size_t>(result);
        }
        return true;
    }

    bool end_file() override {
        if (file_descriptor_ < 0) {
            return false;
        }
        const bool success = close(file_descriptor_) == 0;
        file_descriptor_ = -1;
        current_path_.clear();
        return success;
    }

    void abort_file() override {
        if (file_descriptor_ >= 0) {
            close(file_descriptor_);
            file_descriptor_ = -1;
        }
        if (!current_path_.empty()) {
            unlink(current_path_.c_str());
            current_path_.clear();
        }
    }

private:
    std::string root_;
    std::string current_path_;
    int file_descriptor_ = -1;
};

int validate(int file_descriptor, std::uint64_t image_size,
             std::array<std::uint8_t, 0x200>& header) {
    if (file_descriptor < 0 || image_size == 0 || !read_header(file_descriptor, header)) {
        return 1;
    }
    const auto plan = ykw2::core::build_import_plan(
        ykw2::core::ByteView(header.data(), header.size()), image_size, true);
    if (plan) {
        return 0;
    }
    if (plan.error->code == "invalid_ncsd_magic") {
        return 2;
    }
    return 3;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_ykw2_port_RomImportBridge_nativeValidateRom(
    JNIEnv*, jobject, jint file_descriptor, jlong image_size) {
    std::array<std::uint8_t, 0x200> header{};
    const auto result = validate(file_descriptor, static_cast<std::uint64_t>(image_size), header);
    close(file_descriptor);
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_ykw2_port_RomImportBridge_nativeImportRom(
    JNIEnv* environment, jobject, jint file_descriptor, jlong image_size, jstring cache_directory) {
    if (file_descriptor < 0 || image_size <= 0 || cache_directory == nullptr) {
        close(file_descriptor);
        return 1;
    }
    const auto* cache_chars = environment->GetStringUTFChars(cache_directory, nullptr);
    if (cache_chars == nullptr) {
        close(file_descriptor);
        return 1;
    }
    const std::string cache_path(cache_chars);
    environment->ReleaseStringUTFChars(cache_directory, cache_chars);
    if (!ensure_directory(cache_path)) {
        close(file_descriptor);
        return 1;
    }

    std::array<std::uint8_t, 0x200> header{};
    const auto validation = validate(file_descriptor, static_cast<std::uint64_t>(image_size), header);
    if (validation != 0) {
        close(file_descriptor);
        return validation;
    }
    const auto plan = ykw2::core::build_import_plan(
        ykw2::core::ByteView(header.data(), header.size()),
        static_cast<std::uint64_t>(image_size), true);
    FdSource source(file_descriptor, static_cast<std::uint64_t>(image_size));
    DirectorySink sink(cache_path);
    const auto extraction = ykw2::core::extract_application_romfs(source, *plan.value, sink);
    close(file_descriptor);
    if (!extraction) {
        sink.abort_file();
        return 3;
    }

    const auto marker_path = cache_path + "/.import-complete";
    const auto marker_fd = open(marker_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (marker_fd < 0) {
        return 3;
    }
    const std::string marker = "files=" + std::to_string(extraction.value->files) +
                               "\nbytes=" + std::to_string(extraction.value->bytes) + "\n";
    const auto marker_result = ::write(marker_fd, marker.data(), marker.size());
    const bool marker_ok = marker_result == static_cast<ssize_t>(marker.size()) && close(marker_fd) == 0;
    return marker_ok ? 0 : 3;
}
