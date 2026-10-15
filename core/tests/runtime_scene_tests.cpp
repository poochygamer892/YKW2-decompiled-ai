#include "ykw2/core/runtime_scene.hpp"

#include <cassert>

namespace {

ykw2::core::RuntimeBootInfo boot(ykw2::core::RuntimeBootState state) {
    ykw2::core::RuntimeBootInfo result;
    result.state = state;
    return result;
}

void test_import_screen() {
    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::ImportRequired));
    const auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Import);
    assert(snapshot.status_key == "import.required");
    const auto transition = scene.handle(ykw2::core::RuntimeInput::Confirm);
    assert(transition.handled);
    assert(transition.command == ykw2::core::RuntimeCommand::RequestAuthorizedImport);
}

void test_incomplete_import_screen() {
    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::ImportIncomplete));
    assert(scene.snapshot().status_key == "import.incomplete");
}

void test_title_menu_navigation() {
    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready));
    auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::TitleMenu);
    assert(snapshot.items.size() == 3);
    assert(snapshot.selected_index == 0);
    assert(snapshot.presentation.id == "ykw2_mobile");
    assert(snapshot.presentation.reference_style == "yw1_smartphone");
    assert(snapshot.presentation.portrait_preferred);
    assert(snapshot.presentation.touch_controls);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find("screen=title_menu\n") == 0);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "presentation_id=ykw2_mobile\n") != std::string::npos);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    snapshot = scene.snapshot();
    // The unavailable Continue item is skipped by navigation.
    assert(snapshot.selected_index == 2);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::OpenSettings);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    assert(scene.snapshot().selected_index == 0);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::StartNewGame);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::Dialogue);
    assert(ykw2::core::serialize_runtime_response(
               {true, ykw2::core::RuntimeCommand::StartNewGame}, snapshot)
               .find("command=start_new_game\n") == 0);
}

void test_title_menu_waits_for_initial_content() {
    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), false, 0);
    const auto snapshot = scene.snapshot();
    assert(snapshot.status_key == "runtime.content_missing");
    assert(!snapshot.initial_content_ready);
    assert(!snapshot.items[0].enabled);
    assert(!scene.handle(ykw2::core::RuntimeInput::Confirm).handled);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find("content_ready=0\n") !=
           std::string::npos);
}

void test_title_menu_exposes_scene_resource_plan() {
    ykw2::core::InitialContentInfo content;
    content.archive_files = 3;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 12});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.opening_event_program = ykw2::core::Xq32ProgramInfo{};
    content.opening_event_program->header.instruction_count = 8;
    content.opening_event_program->symbols = {"Main", "ev01_0100_010"};

    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    const auto snapshot = scene.snapshot();
    assert(snapshot.scene_resources.size() == 1);
    assert(snapshot.scene_resources[0].path == "data/map/e100g002/e100g002.xc");
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "scene_resource=map|data/map/e100g002/e100g002.xc|30|100|2|12\n") !=
           std::string::npos);
}

void test_new_game_enters_world_and_moves_player() {
    ykw2::core::InitialContentInfo content;
    content.archive_files = 3;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 12});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.opening_event_program = ykw2::core::Xq32ProgramInfo{};
    content.opening_event_program->header.instruction_count = 8;
    content.opening_event_program->header.argument_count = 20;
    content.opening_event_program->symbols = {"Main", "ev01_0100_010"};

    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    const auto transition = scene.handle(ykw2::core::RuntimeInput::Confirm);
    assert(transition.command == ykw2::core::RuntimeCommand::StartNewGame);
    auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Dialogue);
    assert(snapshot.dialogue.active);
    assert(snapshot.dialogue.event_id == "seq/event/ev01_0100.xq");
    assert(snapshot.opening_event_program.has_value());
    assert(snapshot.opening_event_program->header.instruction_count == 8);
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::World);
    assert(snapshot.status_key == "runtime.world");
    assert(snapshot.world.active);
    assert(snapshot.world.map_resource == "data/map/e100g002/e100g002.xc");

    scene.handle(ykw2::core::RuntimeInput::MoveRight);
    scene.handle(ykw2::core::RuntimeInput::MoveDown);
    snapshot = scene.snapshot();
    assert(snapshot.world.player_x == 1.0f);
    assert(snapshot.world.player_y == 1.0f);
    assert(snapshot.world.frame == 2);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find("screen=world\n") == 0);

    scene.handle(ykw2::core::RuntimeInput::Back);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::TitleMenu);
}

void test_dialogue_traces_decoded_xq32_rows() {
    ykw2::core::InitialContentInfo content;
    content.archive_files = 1;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 1});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.opening_event_program = ykw2::core::Xq32ProgramInfo{};
    content.opening_event_program->header.instruction_count = 2;
    content.opening_event_program->symbols = {"Main"};
    content.opening_event_program->instruction_records = {
        {{0x00020000u, 0x00140689u, 0u}},
        {{0x00020002u, 0x00140689u, 0u}},
    };

    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::StartNewGame);
    auto snapshot = scene.snapshot();
    assert(snapshot.dialogue.line_count == 2);
    assert(snapshot.dialogue.speaker == "Main");
    assert(snapshot.dialogue.text_key == "xq32.step.1");
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "event_decoded_instructions=2\n") != std::string::npos);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "event_instruction_row=0|131072|1312393|0\n") != std::string::npos);

    scene.handle(ykw2::core::RuntimeInput::Confirm);
    snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Dialogue);
    assert(snapshot.dialogue.line_index == 1);
    assert(snapshot.dialogue.text_key == "xq32.step.2");
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::World);
}

void test_world_state_can_be_saved_and_restored() {
    ykw2::core::InitialContentInfo content;
    content.archive_files = 3;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 12});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";

    ykw2::core::RuntimeScene source;
    source.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    source.handle(ykw2::core::RuntimeInput::Confirm);
    source.handle(ykw2::core::RuntimeInput::Confirm);
    source.handle(ykw2::core::RuntimeInput::Confirm);
    source.handle(ykw2::core::RuntimeInput::Confirm);
    source.handle(ykw2::core::RuntimeInput::MoveRight);
    const auto saved = source.save_state();
    assert(saved.has_value());
    assert(saved->player_x == 1.0f);

    const auto bytes = ykw2::core::serialize_runtime_save(*saved);
    const auto parsed = ykw2::core::parse_runtime_save(bytes);
    assert(parsed.has_value());

    ykw2::core::RuntimeScene restored;
    restored.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    assert(restored.restore_state(*parsed));
    const auto snapshot = restored.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::World);
    assert(snapshot.world.player_x == 1.0f);
    assert(snapshot.world.frame == 1);
}

void test_world_does_not_fabricate_a_battle() {
    ykw2::core::InitialContentInfo content;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 12});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.runtime_content.battle_config_path = "data/res/battle/battle_config_0.09k.cfg.bin";
    content.runtime_content.battle_rule_path = "data/res/battle/battle_rule_config_0.03g.cfg.bin";
    content.runtime_content.battle_command_path = "data/res/battle/battle_command_0.04d.cfg.bin";
    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    assert(scene.snapshot().screen == ykw2::core::RuntimeScreen::World);
    assert(!scene.snapshot().battle.active);
}

void test_world_enters_data_backed_battle_catalog() {
    ykw2::core::InitialContentInfo content;
    content.scene_resources.push_back({
        ykw2::core::InitialSceneResourceRole::Map,
        "data/map/e100g002/e100g002.xc", 100, 30,
        ykw2::core::ResourceKind::Xpck, 12});
    content.runtime_content.opening_event_path = "seq/event/ev01_0100.xq";
    content.runtime_content.battle_config_path = "battle.cfg";
    content.runtime_content.battle_rule_path = "rule.cfg";
    content.runtime_content.battle_command_path = "command.cfg";
    content.battle_content.ready = true;
    content.battle_content.battle_sequence_ids = {"N40_norm_battle_intro"};
    content.battle_content.command_ids = {"sp_101000", "sp_102000"};

    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    scene.handle(ykw2::core::RuntimeInput::Confirm);
    const auto transition = scene.handle(ykw2::core::RuntimeInput::Confirm);
    assert(transition.handled);
    assert(transition.command == ykw2::core::RuntimeCommand::StartBattle);
    const auto snapshot = scene.snapshot();
    assert(snapshot.screen == ykw2::core::RuntimeScreen::Battle);
    assert(snapshot.battle.definition_loaded);
    assert(snapshot.battle.definition_id == "N40_norm_battle_intro");
    assert(snapshot.battle.configured_command_count == 2);
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "battle_sequence_id=N40_norm_battle_intro\n") != std::string::npos);
}

}  // namespace

int main() {
    test_import_screen();
    test_incomplete_import_screen();
    test_title_menu_navigation();
    test_title_menu_waits_for_initial_content();
    test_title_menu_exposes_scene_resource_plan();
    test_new_game_enters_world_and_moves_player();
    test_dialogue_traces_decoded_xq32_rows();
    test_world_state_can_be_saved_and_restored();
    test_world_does_not_fabricate_a_battle();
    test_world_enters_data_backed_battle_catalog();
    return 0;
}
