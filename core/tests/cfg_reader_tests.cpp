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
    test_rejects_short_header();
    return 0;
}
