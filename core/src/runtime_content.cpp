#include "ykw2/core/runtime_content.hpp"

#include <array>
#include <string_view>

namespace ykw2::core {
namespace {

bool contains(const Arc0Archive& archive, std::string_view path) {
    return archive.find(path) != nullptr;
}

}  // namespace

RuntimeContentManifest select_runtime_content_manifest(const Arc0Archive& archive) {
    RuntimeContentManifest manifest;
    if (contains(archive, "seq/menu/TitleMenu.xq")) {
        manifest.title_sequence_path = "seq/menu/TitleMenu.xq";
    }
    if (contains(archive, "seq/menu/MainMenu.xq")) {
        manifest.main_menu_sequence_path = "seq/menu/MainMenu.xq";
    }
    // This identifies the first verified numbered opening-event program. It
    // is deliberately not converted into invented dialogue by the core.
    if (contains(archive, "seq/event/ev01_0100.xq")) {
        manifest.opening_event_path = "seq/event/ev01_0100.xq";
    }
    if (contains(archive, "data/res/battle/battle_config_0.09k.cfg.bin")) {
        manifest.battle_config_path = "data/res/battle/battle_config_0.09k.cfg.bin";
    }
    if (contains(archive, "data/res/battle/battle_rule_config_0.03g.cfg.bin")) {
        manifest.battle_rule_path = "data/res/battle/battle_rule_config_0.03g.cfg.bin";
    }
    if (contains(archive, "data/res/battle/battle_command_0.04d.cfg.bin")) {
        manifest.battle_command_path = "data/res/battle/battle_command_0.04d.cfg.bin";
    }
    if (contains(archive, "data/res/battle/battle_view_config_0.01n.cfg.bin")) {
        manifest.battle_view_config_path =
            "data/res/battle/battle_view_config_0.01n.cfg.bin";
    }
    if (contains(archive, "data/res/battle/battle_ai_0.01.cfg.bin")) {
        manifest.battle_ai_path = "data/res/battle/battle_ai_0.01.cfg.bin";
    }
    if (contains(archive, "data/res/map/map_sound_0.03c.cfg.bin")) {
        manifest.map_sound_config_path = "data/res/map/map_sound_0.03c.cfg.bin";
    }
    if (contains(archive, "data/snd/cfg/event/ev01_0100.cfg.bin")) {
        manifest.opening_event_sound_config_path =
            "data/snd/cfg/event/ev01_0100.cfg.bin";
    }
    constexpr std::array<std::string_view, 5> kBattleTextures = {
        "data/menu/img/battle_parts00.xi",
        "data/menu/img/battle_parts01.xi",
        "data/menu/img/battle_parts02.xi",
        "data/menu/img/battle_parts03.xi",
        "data/menu/img/mm_hpbar.xi",
    };
    for (const auto path : kBattleTextures) {
        if (contains(archive, path)) {
            manifest.battle_texture_paths.emplace_back(path);
        }
    }
    return manifest;
}

}  // namespace ykw2::core
