#include "ykw2/core/xq32_reader.hpp"

#include <algorithm>
#include <array>
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

class MemoryStore final : public ykw2::core::ResourceStore {
public:
    explicit MemoryStore(std::vector<std::uint8_t> program) : program_(std::move(program)) {
        std::array<std::uint8_t, 4> signature{};
        std::copy_n(program_.data(), signature.size(), signature.data());
        const auto catalog = ykw2::core::build_resource_catalog(
            {{"opening.xq", program_.size(), signature}});
        assert(catalog);
        catalog_ = *catalog.value;
    }

    const ykw2::core::ResourceCatalog& catalog() const override { return catalog_; }

    bool read(std::string_view path, std::uint64_t offset, std::uint8_t* destination,
              std::size_t length) const override {
        if (path != "opening.xq" || offset > program_.size() ||
            length > program_.size() - offset) {
            return false;
        }
        std::copy_n(program_.data() + offset, length, destination);
        return true;
    }

private:
    std::vector<std::uint8_t> program_;
    ykw2::core::ResourceCatalog catalog_;
};

std::vector<std::uint8_t> program() {
    std::vector<std::uint8_t> bytes(0x40, 0);
    bytes[0] = 'X'; bytes[1] = 'Q'; bytes[2] = '3'; bytes[3] = '2';
    put16(bytes, 0x04, 1);
    put16(bytes, 0x06, 6);   // functions: 0x18..0x24
    put16(bytes, 0x08, 9);   // jumps: 0x24..0x24
    put16(bytes, 0x0a, 0);
    put16(bytes, 0x0c, 9);   // instructions: 0x24..0x2c
    put16(bytes, 0x0e, 1);
    put16(bytes, 0x10, 11);  // arguments: 0x2c..0x34
    put16(bytes, 0x12, 1);
    put16(bytes, 0x14, 2);
    put16(bytes, 0x16, 13);  // symbols: 0x34
    bytes[0x34] = 'M'; bytes[0x35] = 'a'; bytes[0x36] = 'i'; bytes[0x37] = 'n';
    bytes[0x38] = 0;
    bytes[0x39] = 'E'; bytes[0x3a] = 'v'; bytes[0x3b] = '0'; bytes[0x3c] = '1';
    bytes[0x3d] = 0;
    return bytes;
}

void test_reads_metadata_and_symbols() {
    MemoryStore store(program());
    const auto result = ykw2::core::read_xq32_program(store, "opening.xq");
    assert(result);
    assert(result.value->header.instruction_count == 1);
    assert(result.value->function_table_bytes == 12);
    assert(result.value->instruction_table_bytes == 8);
    assert(result.value->argument_table_bytes == 8);
    assert((result.value->symbols == std::vector<std::string>{"Main", "Ev01"}));
}

void test_rejects_non_printable_symbol_data() {
    auto bytes = program();
    bytes[0x35] = 0x80;
    MemoryStore store(std::move(bytes));
    const auto result = ykw2::core::read_xq32_program(store, "opening.xq");
    assert(!result);
    assert(result.error->code == "xq32_symbol_not_printable");
}

}  // namespace

int main() {
    test_reads_metadata_and_symbols();
    test_rejects_non_printable_symbol_data();
    return 0;
}
