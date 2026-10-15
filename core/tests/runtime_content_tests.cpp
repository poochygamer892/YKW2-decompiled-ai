#include "ykw2/core/runtime_content.hpp"

#include <cassert>
#include <string>

namespace {

ykw2::core::Arc0Archive make_archive() {
    ykw2::core::Arc0Archive archive;
    for (const char* path : {
             "seq/menu/TitleMenu.xq",
             "seq/menu/MainMenu.xq",
             "seq/event/ev01_0100.xq",
             "data/res/battle/battle_config_0.09k.cfg.bin",
             "data/res/battle/battle_rule_config_0.03g.cfg.bin",
             "data/res/battle/battle_command_0.04d.cfg.bin",
             "data/res/battle/battle_view_config_0.01n.cfg.bin",
             "data/res/battle/battle_ai_0.01.cfg.bin",
             "data/res/map/map_sound_0.03c.cfg.bin",
             "data/snd/cfg/event/ev01_0100.cfg.bin",
             "data/menu/img/battle_parts00.xi",
             "data/menu/img/battle_parts02.xi",
             "data/menu/img/mm_hpbar.xi",
         }) {
        archive.files.push_back({std::string(path), 0, 1, 0});
    }
    return archive;
}

void test_selects_verified_yw2_bootstrap_paths() {
    const auto manifest = ykw2::core::select_runtime_content_manifest(make_archive());
    assert(manifest.title_sequence_path == "seq/menu/TitleMenu.xq");
    assert(manifest.main_menu_sequence_path == "seq/menu/MainMenu.xq");
    assert(manifest.opening_event_path == "seq/event/ev01_0100.xq");
    assert(manifest.has_opening_event());
    assert(manifest.has_battle_definition());
    assert(manifest.has_battle_presentation());
    assert(manifest.has_audio_definition());
    assert(manifest.battle_view_config_path ==
           "data/res/battle/battle_view_config_0.01n.cfg.bin");
    assert(manifest.battle_ai_path == "data/res/battle/battle_ai_0.01.cfg.bin");
    assert(manifest.map_sound_config_path == "data/res/map/map_sound_0.03c.cfg.bin");
    assert(manifest.opening_event_sound_config_path ==
           "data/snd/cfg/event/ev01_0100.cfg.bin");
    assert(manifest.battle_texture_paths.size() == 3);
}

void test_does_not_fabricate_missing_paths() {
    const ykw2::core::Arc0Archive archive;
    const auto manifest = ykw2::core::select_runtime_content_manifest(archive);
    assert(!manifest.has_opening_event());
    assert(!manifest.has_battle_definition());
    assert(!manifest.has_battle_presentation());
    assert(!manifest.has_audio_definition());
    assert(manifest.battle_texture_paths.empty());
}

}  // namespace

int main() {
    test_selects_verified_yw2_bootstrap_paths();
    test_does_not_fabricate_missing_paths();
    return 0;
}
