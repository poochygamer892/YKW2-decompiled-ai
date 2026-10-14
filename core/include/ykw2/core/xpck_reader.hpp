#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/formats.hpp"
#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

struct XpckFile {
    // XPCK names are ASCII in the observed indexes; preserve bytes as stored.
    std::string name;
    std::uint32_t hash = 0;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
};

struct XpckArchive {
    XpckHeader header;
    std::vector<XpckFile> files;

    const XpckFile* find(std::string_view name) const;
};

struct XpckReadOptions {
    std::uint64_t max_compressed_table_bytes = 64ull * 1024ull * 1024ull;
    std::uint64_t max_decoded_name_bytes = 256ull * 1024ull * 1024ull;
    std::uint64_t max_entries = 1ull * 1024ull * 1024ull;
};

ParseResult<XpckArchive> read_xpck_archive(
    const ResourceStore& store, std::string_view path,
    XpckReadOptions options = {});

}  // namespace ykw2::core
