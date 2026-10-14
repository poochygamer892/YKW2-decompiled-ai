#include "ykw2/core/romfs_extract.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

class MemorySource final : public ykw2::core::ResourceSource {
public:
    explicit MemorySource(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

    std::uint64_t size() const override { return data_.size(); }

    bool read(std::uint64_t offset, std::uint8_t* destination,
              std::size_t length) const override {
        if (!contains(offset, length)) {
            return false;
        }
        std::copy_n(data_.data() + offset, length, destination);
        return true;
    }

private:
    std::vector<std::uint8_t> data_;
};

class MemorySink final : public ykw2::core::ResourceSink {
public:
    bool begin_file(std::string_view path, std::uint64_t size) override {
        current_path = std::string(path);
        current.clear();
        current.reserve(static_cast<std::size_t>(size));
        return true;
    }

    bool write(const std::uint8_t* data, std::size_t size) override {
        current.insert(current.end(), data, data + size);
        return true;
    }

    bool end_file() override {
        files.emplace_back(current_path, current);
        current_path.clear();
        current.clear();
        return true;
    }

    std::string current_path;
    std::vector<std::uint8_t> current;
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
};

void test_lv3_extraction() {
    std::vector<std::uint8_t> romfs(0x100, 0);
    // RomFS Level 3 header.
    put32(romfs, 0x00, 0x28);
    put32(romfs, 0x04, 0x28);
    put32(romfs, 0x08, 0);
    put32(romfs, 0x0c, 0x28);
    put32(romfs, 0x10, 0x18);
    put32(romfs, 0x14, 0x40);
    put32(romfs, 0x18, 0);
    put32(romfs, 0x1c, 0x40);
    put32(romfs, 0x20, 0x2a);
    put32(romfs, 0x24, 0x80);

    // Root directory metadata points at the first file and no child directory.
    put32(romfs, 0x28 + 0x08, 0xffffffffu);
    put32(romfs, 0x28 + 0x0c, 0);

    // One file named "hello", stored at the beginning of the file-data area.
    put32(romfs, 0x40 + 0x04, 0xffffffffu);
    put32(romfs, 0x40 + 0x08, 0);
    put32(romfs, 0x40 + 0x10, 5);
    put32(romfs, 0x40 + 0x1c, 10);
    for (std::size_t index = 0; index < 5; ++index) {
        put16(romfs, 0x40 + 0x20 + index * 2, static_cast<std::uint16_t>("hello"[index]));
    }
    std::copy_n("world", 5, romfs.begin() + 0x80);

    MemorySource source(std::move(romfs));
    MemorySink sink;
    const auto result = ykw2::core::extract_romfs(source, 0, 0x100, sink);
    assert(result);
    assert(result.value->files == 1);
    assert(result.value->bytes == 5);
    assert(sink.files.size() == 1);
    assert(sink.files[0].first == "hello");
    assert(std::string(sink.files[0].second.begin(), sink.files[0].second.end()) == "world");
}

void test_ncch_header() {
    std::vector<std::uint8_t> header(0x200, 0);
    header[0x100] = 'N';
    header[0x101] = 'C';
    header[0x102] = 'C';
    header[0x103] = 'H';
    put32(header, 0x104, 0x100);
    put32(header, 0x1b0, 1);
    put32(header, 0x1b4, 2);
    header[0x18f] = 0x04;  // no_crypto
    const auto parsed = ykw2::core::parse_ncch_header(
        ykw2::core::ByteView(header.data(), header.size()), 0x20000);
    assert(parsed);
    assert(parsed.value->romfs_offset == 0x200);
    assert(parsed.value->romfs_size == 0x400);
    assert(parsed.value->no_crypto);
}

}  // namespace

int main() {
    test_lv3_extraction();
    test_ncch_header();
    return 0;
}
