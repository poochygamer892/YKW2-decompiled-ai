#include "ykw2/core/cfg_reader.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

class MemoryStore final : public ykw2::core::ResourceStore {
public:
    explicit MemoryStore(std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {
        std::array<std::uint8_t, 4> signature{};
        std::copy_n(bytes_.data(), std::min<std::size_t>(4, bytes_.size()),
                    signature.data());
        const auto result = ykw2::core::build_resource_catalog(
            {ykw2::core::CatalogInput{"table.cfg.bin", bytes_.size(), signature}});
        assert(result);
        catalog_ = std::move(*result.value);
    }

    const ykw2::core::ResourceCatalog& catalog() const override { return catalog_; }

    bool read(std::string_view path, std::uint64_t offset,
              std::uint8_t* destination, std::size_t length) const override {
        if (path != "table.cfg.bin" || offset > bytes_.size() ||
            length > bytes_.size() - offset) {
            return false;
        }
        std::copy_n(bytes_.data() + offset, length, destination);
        return true;
    }

private:
    std::vector<std::uint8_t> bytes_;
    ykw2::core::ResourceCatalog catalog_;
};

void test_reads_raw_structural_header() {
    std::vector<std::uint8_t> bytes(0x20, 0);
    put32(bytes, 0x00, 0x1e6);
    put32(bytes, 0x04, 0x2630);
    put32(bytes, 0x08, 0x161);
    put32(bytes, 0x0c, 0x15);
    const auto parsed = ykw2::core::parse_cfg_binary_header(
        ykw2::core::ByteView(bytes.data(), 0x10), bytes.size());
    assert(parsed);
    assert(parsed.value->file_size == 0x20);
    assert(parsed.value->field_0x00 == 0x1e6);
    assert(parsed.value->field_0x04 == 0x2630);
    assert(parsed.value->field_0x08 == 0x161);
    assert(parsed.value->field_0x0c == 0x15);
    assert(parsed.value->readable_header_bytes == 0x10);
}

void test_reads_from_resource_store() {
    std::vector<std::uint8_t> bytes(0x20, 0);
    put32(bytes, 0x00, 1);
    put32(bytes, 0x04, 2);
    put32(bytes, 0x08, 3);
    put32(bytes, 0x0c, 4);
    MemoryStore store(std::move(bytes));
    const auto parsed = ykw2::core::read_cfg_binary_info(store, "table.cfg.bin");
    assert(parsed);
    assert(parsed.value->field_0x0c == 4);
}

void test_decodes_packed_types_and_strings() {
    std::vector<std::uint8_t> bytes(0x40, 0);
    put32(bytes, 0x00, 2);      // entries
    put32(bytes, 0x04, 0x30);   // string table offset
    put32(bytes, 0x08, 6);      // string table length
    put32(bytes, 0x0c, 1);      // string count
    put32(bytes, 0x10, 0x11111111);
    bytes[0x14] = 3;
    bytes[0x15] = 0x24;         // string, int, float
    put32(bytes, 0x18, 0);      // string offset
    put32(bytes, 0x1c, 42);
    put32(bytes, 0x20, 0x3f800000); // 1.0f
    put32(bytes, 0x24, 0x22222222);
    bytes[0x28] = 1;
    bytes[0x29] = 0x01;         // int
    put32(bytes, 0x2c, 7);
    const std::string text = "hello";
    std::copy(text.begin(), text.end(), bytes.begin() + 0x30);

    const auto parsed = ykw2::core::parse_cfg_binary(
        ykw2::core::ByteView(bytes.data(), bytes.size()), bytes.size());
    assert(parsed);
    assert(parsed.value->entries.size() == 2);
    assert(parsed.value->entries[0].values.size() == 3);
    assert(parsed.value->entries[0].values[0].type ==
           ykw2::core::CfgValueType::String);
    assert(parsed.value->entries[0].values[0].string_bytes == "hello");
    assert(parsed.value->entries[0].values[1].raw_value == 42);
    assert(parsed.value->entries[0].values[2].raw_value == 0x3f800000);
    assert(parsed.value->entries[1].values[0].raw_value == 7);
}

void test_rejects_short_header() {
    const std::array<std::uint8_t, 8> bytes{};
    const auto parsed = ykw2::core::parse_cfg_binary_header(
        ykw2::core::ByteView(bytes.data(), bytes.size()), bytes.size());
    assert(!parsed);
    assert(parsed.error->code == "cfg_header_truncated");
}

}  // namespace

int main() {
    test_reads_raw_structural_header();
    test_reads_from_resource_store();
    test_decodes_packed_types_and_strings();
    test_rejects_short_header();
    return 0;
}
