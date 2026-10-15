#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/byte_view.hpp"
#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

// Level-5 CFG files do not have a single magic value shared by every table.
// Keep this reader deliberately structural: it validates that the resource
// has a readable fixed prefix and exposes raw words without assigning
// unverified table semantics to them.
struct CfgBinaryInfo {
    std::uint64_t file_size = 0;
    std::uint32_t field_0x00 = 0;
    std::uint32_t field_0x04 = 0;
    std::uint32_t field_0x08 = 0;
    std::uint32_t field_0x0c = 0;
    std::uint32_t readable_header_bytes = 0;
};

enum class CfgValueType : std::uint8_t {
    String = 0,
    Int = 1,
    Float = 2,
};

struct CfgValue {
    CfgValueType type = CfgValueType::Int;
    // Integers and floats retain their original little-endian bits. For a
    // string this is the offset relative to field_0x04, or 0xffffffff for a
    // null string.
    std::uint32_t raw_value = 0;
    // Strings remain raw bytes because tables can use different encodings.
    std::string string_bytes;
};

struct CfgEntry {
    std::uint32_t hash = 0;
    std::vector<CfgValue> values;
};

struct CfgBinary {
    CfgBinaryInfo header;
    std::vector<CfgEntry> entries;
};

struct CfgReadOptions {
    std::uint32_t max_entries = 1u * 1024u * 1024u;
    std::uint64_t max_file_bytes = 64ull * 1024ull * 1024ull;
    std::uint64_t max_string_bytes = 64ull * 1024ull * 1024ull;
};

ParseResult<CfgBinaryInfo> parse_cfg_binary_header(ByteView bytes,
                                                   std::uint64_t file_size);

ParseResult<CfgBinary> parse_cfg_binary(
    ByteView bytes, std::uint64_t file_size, CfgReadOptions options = {});

ParseResult<CfgBinaryInfo> read_cfg_binary_info(const ResourceStore& store,
                                                std::string_view path);

ParseResult<CfgBinary> read_cfg_binary(const ResourceStore& store,
                                       std::string_view path,
                                       CfgReadOptions options = {});

}  // namespace ykw2::core
