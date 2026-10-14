#include <jni.h>

#include <array>
#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "ykw2/core/import_contract.hpp"
#include "ykw2/core/resource_catalog.hpp"
#include "ykw2/core/romfs_extract.hpp"
#include "ykw2/core/runtime_bootstrap.hpp"
#include "ykw2/core/runtime_scene.hpp"

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
        current_relative_path_ = std::string(path);
        current_size_ = 0;
        current_signature_.fill(0);
        current_signature_size_ = 0;
        current_path_ = root_ + "/" + current_relative_path_;
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
        const auto signature_bytes = std::min<std::size_t>(
            current_signature_.size() - current_signature_size_, size);
        std::copy_n(data, signature_bytes, current_signature_.data() + current_signature_size_);
        current_signature_size_ += signature_bytes;
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
        current_size_ += size;
        return true;
    }

    bool end_file() override {
        if (file_descriptor_ < 0) {
            return false;
        }
        const bool success = close(file_descriptor_) == 0;
        file_descriptor_ = -1;
        if (success) {
            inputs_.push_back({current_relative_path_, current_size_, current_signature_});
        }
        current_relative_path_.clear();
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
            current_relative_path_.clear();
            current_path_.clear();
        }
    }

    const std::vector<ykw2::core::CatalogInput>& inputs() const { return inputs_; }

private:
    std::string root_;
    std::string current_path_;
    std::string current_relative_path_;
    std::array<std::uint8_t, 4> current_signature_{};
    std::size_t current_signature_size_ = 0;
    std::uint64_t current_size_ = 0;
    int file_descriptor_ = -1;
    std::vector<ykw2::core::CatalogInput> inputs_;
};

bool write_text_file(const std::string& path, const std::string& contents) {
    const auto descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0) {
        return false;
    }
    std::size_t written = 0;
    while (written < contents.size()) {
        const auto result = ::write(descriptor, contents.data() + written,
                                    contents.size() - written);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            close(descriptor);
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return close(descriptor) == 0;
}

bool read_text_file(const std::string& path, std::string& contents) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

bool marker_value(std::string_view marker, std::string_view key, std::uint64_t& value) {
    std::size_t cursor = 0;
    while (cursor <= marker.size()) {
        const auto end = marker.find('\n', cursor);
        const auto line = marker.substr(cursor, end == std::string_view::npos
                                                  ? marker.size() - cursor
                                                  : end - cursor);
        const auto prefix = std::string(key) + "=";
        if (line.substr(0, prefix.size()) == prefix) {
            const auto text = line.substr(prefix.size());
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }
        if (end == std::string_view::npos) {
            break;
        }
        cursor = end + 1;
    }
    return false;
}

int load_runtime_boot(const std::string& cache_path, ykw2::core::RuntimeBootInfo& boot) {
    std::string catalog_text;
    std::string marker_text;
    if (!read_text_file(cache_path + "/.catalog", catalog_text) ||
        !read_text_file(cache_path + "/.import-complete", marker_text)) {
        boot.state = ykw2::core::RuntimeBootState::ImportRequired;
        return 1;
    }
    const auto catalog = ykw2::core::parse_resource_catalog(catalog_text);
    if (!catalog) {
        boot.state = ykw2::core::RuntimeBootState::ImportIncomplete;
        return 2;
    }
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t fingerprint = 0;
    if (!marker_value(marker_text, "files", files) ||
        !marker_value(marker_text, "bytes", bytes) ||
        !marker_value(marker_text, "fingerprint", fingerprint)) {
        boot.state = ykw2::core::RuntimeBootState::ImportIncomplete;
        return 2;
    }
    boot = ykw2::core::evaluate_runtime_boot(
        *catalog.value, true, files, bytes, fingerprint);
    return boot.state == ykw2::core::RuntimeBootState::Ready ? 0 : 2;
}

int check_import(const std::string& cache_path) {
    ykw2::core::RuntimeBootInfo boot;
    return load_runtime_boot(cache_path, boot);
}

std::mutex runtime_mutex;
ykw2::core::RuntimeScene runtime_scene;
bool runtime_started = false;

std::string start_runtime(const std::string& cache_path) {
    ykw2::core::RuntimeBootInfo boot;
    load_runtime_boot(cache_path, boot);
    runtime_scene.start(boot);
    runtime_started = true;
    return ykw2::core::serialize_runtime_response(
        {false, ykw2::core::RuntimeCommand::None}, runtime_scene.snapshot());
}

std::string handle_runtime_input(jint input) {
    if (!runtime_started) {
        ykw2::core::RuntimeBootInfo boot;
        boot.state = ykw2::core::RuntimeBootState::ImportRequired;
        runtime_scene.start(boot);
        runtime_started = true;
    }
    ykw2::core::RuntimeInput runtime_input;
    switch (input) {
        case 0:
            runtime_input = ykw2::core::RuntimeInput::MoveNext;
            break;
        case 1:
            runtime_input = ykw2::core::RuntimeInput::MovePrevious;
            break;
        case 2:
            runtime_input = ykw2::core::RuntimeInput::Confirm;
            break;
        case 3:
            runtime_input = ykw2::core::RuntimeInput::Back;
            break;
        default:
            return ykw2::core::serialize_runtime_response(
                {false, ykw2::core::RuntimeCommand::None}, runtime_scene.snapshot());
    }
    const auto transition = runtime_scene.handle(runtime_input);
    return ykw2::core::serialize_runtime_response(transition, runtime_scene.snapshot());
}

jstring to_java_string(JNIEnv* environment, const std::string& value) {
    return environment->NewStringUTF(value.c_str());
}

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
Java_com_ykw2_port_RomImportBridge_nativeCheckImport(
    JNIEnv* environment, jobject, jstring cache_directory) {
    if (cache_directory == nullptr) {
        return 1;
    }
    const auto* cache_chars = environment->GetStringUTFChars(cache_directory, nullptr);
    if (cache_chars == nullptr) {
        return 1;
    }
    const std::string cache_path(cache_chars);
    environment->ReleaseStringUTFChars(cache_directory, cache_chars);
    return check_import(cache_path);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ykw2_port_RomImportBridge_nativeStartRuntime(
    JNIEnv* environment, jobject, jstring cache_directory) {
    if (cache_directory == nullptr) {
        return to_java_string(environment, "command=none\nhandled=0\nscreen=import\nselected=0\nstatus=import.required\n");
    }
    const auto* cache_chars = environment->GetStringUTFChars(cache_directory, nullptr);
    if (cache_chars == nullptr) {
        return to_java_string(environment, "command=none\nhandled=0\nscreen=import\nselected=0\nstatus=import.required\n");
    }
    const std::string cache_path(cache_chars);
    environment->ReleaseStringUTFChars(cache_directory, cache_chars);
    std::lock_guard<std::mutex> lock(runtime_mutex);
    return to_java_string(environment, start_runtime(cache_path));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ykw2_port_RomImportBridge_nativeRuntimeInput(
    JNIEnv* environment, jobject, jint input) {
    std::lock_guard<std::mutex> lock(runtime_mutex);
    return to_java_string(environment, handle_runtime_input(input));
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
    unlink((cache_path + "/.import-complete").c_str());
    unlink((cache_path + "/.catalog").c_str());

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

    const auto catalog = ykw2::core::build_resource_catalog(sink.inputs());
    if (!catalog) {
        return 3;
    }
    const auto catalog_path = cache_path + "/.catalog";
    if (!write_text_file(catalog_path, ykw2::core::serialize_resource_catalog(*catalog.value))) {
        return 3;
    }
    const auto marker_path = cache_path + "/.import-complete";
    const std::string marker = "files=" + std::to_string(extraction.value->files) +
                               "\nbytes=" + std::to_string(extraction.value->bytes) +
                               "\nfingerprint=" + std::to_string(catalog.value->fingerprint()) +
                               "\n";
    return write_text_file(marker_path, marker) ? 0 : 3;
}
