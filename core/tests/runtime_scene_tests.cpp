#include "ykw2/core/runtime_scene.hpp"

#include <cassert>

namespace {

ykw2::core::RuntimeBootInfo ready_boot() {
    ykw2::core::RuntimeBootInfo boot;
    boot.state = ykw2::core::RuntimeBootState::Ready;
    return boot;
}

ykw2::core::InitialContentInfo indexed_content() {
    ykw2::core::InitialContentInfo content;
    content.archive_files = 3;
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.runtime_content.battle_config_path = "battle.cfg";
    content.runtime_content.battle_rule_path = "rule.cfg";
    content.runtime_content.battle_command_path = "command.cfg";
    content.opening_event_program = ykw2::core::Xq32ProgramInfo{};
    content.opening_event_program->instructions = {
        {0, 0, 0x00140689u, 0x0689u, 0x0014u, 0, true},
        {0, 0, 0x00140689u, 0x0689u, 0x0014u, 0, true},
    };
    content.opening_event_program->functions = {{0, 0, "Main", true, true}};
    content.battle_content.ready = true;
    content.battle_content.battle_sequence_ids = {"N40_norm_battle_intro"};
    content.battle_content.command_catalog = {
        {17, 0x11111111u, "sp_101000", {1, 2, 3}},
        {18, 0x22222222u, "sp_102000", {4, 5}},
    };
    return content;
}

void test_import_and_empty_menu() {
    ykw2::core::RuntimeScene scene;
    ykw2::core::RuntimeBootInfo import_boot;
    import_boot.state = ykw2::core::RuntimeBootState::ImportRequired;
    scene.start(import_boot);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::Import);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::RequestAuthorizedImport);

    scene.start(ready_boot(), false, 0);
    const auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::TitleMenu);
    assert(snapshot.items.size() == 4);
    for (const auto& item : snapshot.items) assert(!item.enabled);
    assert(!scene.select_menu_item(2).handled);
    assert(!scene.handle(ykw2::core::RuntimeInput::Confirm).handled);
}

void test_menu_opens_only_verified_previews() {
    ykw2::core::RuntimeScene scene;
    scene.start(ready_boot(), indexed_content());
    auto snapshot = scene.snapshot();
    assert(snapshot.presentation.reference_style == "yw1_smartphone");
    assert(!snapshot.items[0].enabled && !snapshot.items[1].enabled);
    assert(snapshot.items[2].enabled && snapshot.items[3].enabled);
    assert(snapshot.selected_index == 2);
    assert(!scene.select_menu_item(0).handled);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::TitleMenu);
    assert(scene.select_menu_item(2).command == ykw2::core::RuntimeCommand::InspectOpening);
    snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Dialogue);
    assert(snapshot.dialogue.event_id == "seq/event/ev01_0100.xq");
    assert(snapshot.dialogue.speaker.empty());
    assert(snapshot.dialogue.line_count == 2);
    assert(!scene.save_state().has_value());
    assert(!scene.restore_state({true, "data/map/fake.xc", 12, 4, 2, 1}));
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::Dialogue);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).handled);
    assert(scene.snapshot().dialogue.line_index == 1);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).handled);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::TitleMenu);
    assert(!scene.snapshot().world.active);
    assert(!scene.snapshot().dialogue.active);
}

void test_invalid_instruction_does_not_advance() {
    auto content = indexed_content();
    content.opening_event_program->instructions[0].argument_span_valid = false;
    ykw2::core::RuntimeScene scene;
    scene.start(ready_boot(), content);
    assert(scene.select_menu_item(2).handled);
    assert(!scene.handle(ykw2::core::RuntimeInput::Confirm).handled);
    assert(scene.snapshot().opening_event_operation_status ==
           ykw2::core::Xq32OperationStatus::InvalidArguments);
    assert(scene.snapshot().opening_event_execution.instruction_index == 0);
    assert(scene.handle(ykw2::core::RuntimeInput::Back).handled);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::TitleMenu);
}

void test_battle_catalog_is_not_an_encounter() {
    ykw2::core::RuntimeScene scene;
    scene.start(ready_boot(), indexed_content());
    assert(scene.select_menu_item(3).command ==
           ykw2::core::RuntimeCommand::OpenBattleCatalog);
    auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Battle);
    assert(snapshot.battle.result == "catalog_only");
    assert(snapshot.battle.player_hp == 0 && snapshot.battle.enemy_hp == 0);
    assert(scene.handle(ykw2::core::RuntimeInput::Skill).handled);
    snapshot = scene.snapshot();
    assert(snapshot.battle.last_command_id == "sp_102000");
    assert(snapshot.battle.turn == 0);
    assert(scene.handle(ykw2::core::RuntimeInput::Back).handled);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::TitleMenu);
}

}  // namespace

int main() {
    test_import_and_empty_menu();
    test_menu_opens_only_verified_previews();
    test_invalid_instruction_does_not_advance();
    test_battle_catalog_is_not_an_encounter();
}
