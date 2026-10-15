#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/arc0_reader.hpp"
#include "ykw2/core/cfg_reader.hpp"
#include "ykw2/core/resource_store.hpp"
#include "ykw2/core/runtime_content.hpp"
#include "ykw2/core/model_reader.hpp"
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
    std::uint32_t mesh_vertex_count = 0;
    std::uint32_t mesh_triangle_index_count = 0;
    std::uint32_t mesh_material_hash = 0;
    std::vector<float> mesh_preview_positions;
    std::vector<std::uint16_t> mesh_preview_indices;
};

// One row from a decoded YW2 battle catalog. The numeric fields intentionally
// remain raw until the table's field hashes are mapped; the identifier is the
// only value currently proven to be a symbolic command/sequence name.
struct BattleCatalogRecord {
    std::uint32_t entry_index = 0;
    std::uint32_t hash = 0;
    std::string id;
    std::vector<std::uint32_t> raw_values;
};

struct InitialBattleContent {
    bool ready = false;
    std::uint32_t battle_config_entries = 0;
    std::uint32_t battle_rule_entries = 0;
    std::uint32_t battle_command_entries = 0;
    std::uint32_t battle_view_entries = 0;
    std::uint32_t battle_ai_entries = 0;
    std::vector<std::string> battle_sequence_ids;
    std::vector<std::string> command_ids;
    std::vector<std::string> sound_ids;
    std::vector<BattleCatalogRecord> battle_sequence_catalog;
    std::vector<BattleCatalogRecord> command_catalog;
    std::vector<BattleCatalogRecord> sound_catalog;
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
    InitialBattleContent battle_content;
    std::optional<CfgBinaryInfo> battle_config_header;
    std::optional<CfgBinaryInfo> battle_rule_header;
    std::optional<CfgBinaryInfo> battle_command_header;
    std::optional<CfgBinaryInfo> battle_view_config_header;
    std::optional<CfgBinaryInfo> battle_ai_header;
    std::optional<CfgBinaryInfo> map_sound_config_header;
    std::optional<CfgBinaryInfo> opening_event_sound_config_header;
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
