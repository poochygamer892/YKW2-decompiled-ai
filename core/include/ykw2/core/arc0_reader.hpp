#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/formats.hpp"
#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

struct Arc0File {
    // ARC0 names are stored as Shift-JIS bytes. The string preserves those
    // bytes so a platform can convert them for display without data loss.
    std::string path;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t crc32 = 0;
};

struct Arc0Archive {
    Arc0Header header;
    std::vector<Arc0File> files;

    const Arc0File* find(std::string_view path) const;
};

struct Arc0ReadOptions {
    std::uint64_t max_compressed_table_bytes = 64ull * 1024ull * 1024ull;
    std::uint64_t max_decoded_table_bytes = 256ull * 1024ull * 1024ull;
    std::uint64_t max_files = 1ull * 1024ull * 1024ull;
};

ParseResult<Arc0Archive> read_arc0_archive(
    const ResourceStore& store, std::string_view path,
    Arc0ReadOptions options = {});

// Reads a bounded range from one file inside an ARC0 archive. The caller
// supplies the parsed file record; no nested payload is copied implicitly.
bool read_arc0_file_range(const ResourceStore& store, std::string_view archive_path,
                          const Arc0File& file, std::uint64_t offset,
                          std::uint8_t* destination, std::size_t length);

}  // namespace ykw2::core
