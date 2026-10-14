#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ykw2/core/resource_store.hpp"

namespace ykw2::core {

// Validated metadata for the archive needed by the first native scene. The
// payload remains in the platform-private cache; this object only describes
// the parsed index that the runtime can safely use.
struct InitialContentInfo {
    std::string archive_path;
    std::uint64_t archive_files = 0;
    std::string first_file_path;
    std::uint32_t first_file_size = 0;
};

// Finds the YKW2 main ARC0 archive (preferring the canonical file name),
// parses its bounded index and returns metadata for the runtime bootstrap.
ParseResult<InitialContentInfo> load_initial_content(
    const ResourceStore& store, std::string_view preferred_name = "yw2_a.fa");

}  // namespace ykw2::core
