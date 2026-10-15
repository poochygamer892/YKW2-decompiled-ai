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

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
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

std::vector<std::uint8_t> decoded_tables_program() {
    // One row per table is enough to prove that the reader exposes decoded
    // XQ32 data without assigning unverified meanings to its words.
    std::vector<std::uint8_t> bytes(0x4d, 0);
    bytes[0] = 'X'; bytes[1] = 'Q'; bytes[2] = '3'; bytes[3] = '2';
    put16(bytes, 0x04, 1);    // functions
    put16(bytes, 0x06, 6);    // functions: 0x18..0x30 (24 bytes)
    put16(bytes, 0x08, 12);   // jumps: 0x30..0x30
    put16(bytes, 0x0a, 0);    // no jumps
    put16(bytes, 0x0c, 12);   // instructions: 0x30..0x3c (12 bytes)
    put16(bytes, 0x0e, 1);
    put16(bytes, 0x10, 15);   // arguments: 0x3c..0x44 (8 bytes)
    put16(bytes, 0x12, 1);
    put16(bytes, 0x14, 1);    // one global
    put16(bytes, 0x16, 17);   // symbols: 0x44

    for (std::size_t offset = 0x18; offset < 0x30; offset += 4) {
        const auto word = static_cast<std::uint32_t>(offset * 0x101u + 7u);
        bytes[offset] = static_cast<std::uint8_t>(word & 0xffu);
        bytes[offset + 1] = static_cast<std::uint8_t>((word >> 8) & 0xffu);
        bytes[offset + 2] = static_cast<std::uint8_t>((word >> 16) & 0xffu);
        bytes[offset + 3] = static_cast<std::uint8_t>(word >> 24);
    }
    put32(bytes, 0x18, 0);
    put32(bytes, 0x1c, 0x1f1a625au); // CRC32("Main")
    const std::uint32_t instruction_words[] = {0x00010000u, 0x00140689u, 0u};
    for (std::size_t index = 0; index < 3; ++index) {
        const auto word = instruction_words[index];
        const auto offset = 0x30 + index * 4;
        bytes[offset] = static_cast<std::uint8_t>(word & 0xffu);
        bytes[offset + 1] = static_cast<std::uint8_t>((word >> 8) & 0xffu);
        bytes[offset + 2] = static_cast<std::uint8_t>((word >> 16) & 0xffu);
        bytes[offset + 3] = static_cast<std::uint8_t>(word >> 24);
    }
    put16(bytes, 0x3c, 4);
    put16(bytes, 0x3e, 9);
    // Strings are also allowed to be Level-5 wrapped; method 0 is the
    // smallest fixture for exercising that path.
    bytes[0x44] = 0x28; // decoded size 5, method none
    bytes[0x48] = 'M'; bytes[0x49] = 'a'; bytes[0x4a] = 'i'; bytes[0x4b] = 'n';
    bytes[0x4c] = 0;
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

void test_exposes_decoded_table_rows() {
    MemoryStore store(decoded_tables_program());
    const auto result = ykw2::core::read_xq32_program(store, "opening.xq");
    assert(result);
    assert(result.value->function_records.size() == 1);
    assert(result.value->function_records[0].words.size() == 6);
    assert(result.value->functions.size() == 1);
    assert(result.value->functions[0].symbol == "Main");
    assert(result.value->functions[0].symbol_hash_matches);
    assert(result.value->functions[0].instruction_index_valid);
    assert(result.value->jump_records.empty());
    assert(result.value->instruction_records.size() == 1);
    assert(result.value->instruction_records[0].words.size() == 3);
    assert(result.value->instruction_records[0].words[0] == 0x00010000u);
    assert(result.value->instruction_records[0].words[1] == 0x00140689u);
    assert(result.value->instructions.size() == 1);
    assert(result.value->instructions[0].argument_index == 0);
    assert(result.value->instructions[0].argument_count == 1);
    assert(result.value->instructions[0].argument_span_valid);
    assert(result.value->argument_records.size() == 1);
    assert(result.value->argument_records[0].words.size() == 2);
    assert(result.value->arguments.size() == 1);
    assert((result.value->symbols == std::vector<std::string>{"Main"}));
}

}  // namespace

int main() {
    test_reads_metadata_and_symbols();
    test_rejects_non_printable_symbol_data();
    test_exposes_decoded_table_rows();
    return 0;
}
