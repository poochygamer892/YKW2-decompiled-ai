#pragma once

#include <cstdint>
#include <string_view>

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

ParseResult<CfgBinaryInfo> parse_cfg_binary_header(ByteView bytes,
                                                   std::uint64_t file_size);

ParseResult<CfgBinaryInfo> read_cfg_binary_info(const ResourceStore& store,
                                                std::string_view path);

}  // namespace ykw2::core
