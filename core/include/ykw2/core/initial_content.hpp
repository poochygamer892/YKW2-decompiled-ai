#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/arc0_reader.hpp"
#include "ykw2/core/resource_store.hpp"
#include "ykw2/core/runtime_content.hpp"
#include "ykw2/core/xq32_reader.hpp"

namespace ykw2::core {

enum class InitialSceneResourceRole {
    Map,
    Character,
    Background,
};

struct InitialSceneResource {
    InitialSceneResourceRole role = InitialSceneResourceRole::Map;
    std::string path;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    ResourceKind kind = ResourceKind::Unknown;
    std::uint64_t child_count = 0;
    // Populated for the first background IMGC after it has been decoded to
    // row-major RGBA8. The pixel buffer itself lives on InitialContentInfo.
    std::uint16_t texture_width = 0;
    std::uint16_t texture_height = 0;
    std::uint8_t texture_format = 0;
    std::uint64_t texture_rgba_bytes = 0;
};

const char* initial_scene_resource_role_name(InitialSceneResourceRole role);

// Selects only metadata from the ARC0 index. Payload bytes are still served
// lazily by ResourceStore when a renderer is ready to consume them.
std::vector<InitialSceneResource> select_initial_scene_resources(
    const Arc0Archive& archive);

// Validated metadata for the archive needed by the first native scene. The
// payload remains in the platform-private cache; this object only describes
// the parsed index that the runtime can safely use.
struct InitialContentInfo {
    std::string archive_path;
    std::uint64_t archive_files = 0;
    std::string first_file_path;
    std::uint32_t first_file_size = 0;
    RuntimeContentManifest runtime_content;
    // Metadata from the verified opening program. This is not an interpreter.
    std::optional<Xq32ProgramInfo> opening_event_program;
    std::vector<InitialSceneResource> scene_resources;
    std::uint16_t background_width = 0;
    std::uint16_t background_height = 0;
    std::uint8_t background_format = 0;
    std::vector<std::uint8_t> background_rgba;
};

// Finds the YKW2 main ARC0 archive (preferring the canonical file name),
// parses its bounded index and returns metadata for the runtime bootstrap.
ParseResult<InitialContentInfo> load_initial_content(
    const ResourceStore& store, std::string_view preferred_name = "yw2_a.fa");

}  // namespace ykw2::core
