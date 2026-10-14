#pragma once

#include <string>
#include <vector>

#include "ykw2/core/arc0_reader.hpp"

namespace ykw2::core {

// Paths are resolved against the user's imported YW2 archive at runtime. No
// game payload is embedded in the application or this manifest.
struct RuntimeContentManifest {
    std::string title_sequence_path;
    std::string main_menu_sequence_path;
    std::string opening_event_path;
    std::string battle_config_path;
    std::string battle_rule_path;
    std::string battle_command_path;
    std::vector<std::string> battle_texture_paths;

    bool has_opening_event() const { return !opening_event_path.empty(); }
    bool has_battle_definition() const {
        return !battle_config_path.empty() && !battle_rule_path.empty() &&
               !battle_command_path.empty();
    }
};

// Builds a deterministic, data-only bootstrap manifest from the ARC0 index of
// a YW2 dump. A missing path remains empty; callers can then show an accurate
// import/compatibility error instead of substituting fictional gameplay.
RuntimeContentManifest select_runtime_content_manifest(const Arc0Archive& archive);

}  // namespace ykw2::core
