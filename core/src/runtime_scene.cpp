#include "ykw2/core/runtime_scene.hpp"

#include <algorithm>
#include <utility>

namespace ykw2::core {

void RuntimeScene::start(const RuntimeBootInfo& boot) {
    start(boot, boot.state == RuntimeBootState::Ready, 0);
}

void RuntimeScene::start(const RuntimeBootInfo& boot, bool initial_content_ready,
                         std::uint64_t initial_content_files) {
    boot_ = boot;
    initial_content_ready_ = initial_content_ready && boot.state == RuntimeBootState::Ready;
    initial_content_files_ = initial_content_ready_ ? initial_content_files : 0;
    scene_resources_.clear();
    runtime_content_ = {};
    battle_content_ = {};
    opening_event_program_.reset();
    opening_event_cursor_ = {};
    opening_event_operation_status_ = Xq32OperationStatus::Idle;
    background_width_ = 0;
    background_height_ = 0;
    background_format_ = 0;
    background_rgba_.clear();
    dialogue_ = {};
    world_ = {};
    battle_ = {};
    audio_cue_.clear();
    audio_sequence_ = 0;
    selected_index_ = 0;
    items_.clear();
    if (boot.state != RuntimeBootState::Ready) {
        screen_ = RuntimeScreen::Import;
        return;
    }

    screen_ = RuntimeScreen::TitleMenu;
    rebuild_menu();
}

void RuntimeScene::start(const RuntimeBootInfo& boot, const InitialContentInfo& content) {
    start(boot, true, content.archive_files);
    scene_resources_ = content.scene_resources;
    runtime_content_ = content.runtime_content;
    battle_content_ = content.battle_content;
    opening_event_program_ = content.opening_event_program;
    background_width_ = content.background_width;
    background_height_ = content.background_height;
    background_format_ = content.background_format;
    background_rgba_ = content.background_rgba;
    rebuild_menu();
}

void RuntimeScene::rebuild_menu() {
    Xq32ExecutionCursor probe;
    const bool opening_available = initial_content_ready_ &&
        !runtime_content_.opening_event_path.empty() &&
        opening_event_program_.has_value() &&
        probe.start(*opening_event_program_, "Main");
    const bool catalog_available = initial_content_ready_ && battle_content_.ready &&
        (!battle_content_.command_catalog.empty() || !battle_content_.command_ids.empty());
    items_ = {
        {"new_game", "menu.new_game", false},
        {"continue", "menu.continue", false},
        {"inspect_opening", "menu.inspect_opening", opening_available},
        {"battle_catalog", "menu.battle_catalog", catalog_available},
    };
    selected_index_ = opening_available ? 2 : catalog_available ? 3 : 0;
}

void RuntimeScene::return_to_menu() {
    screen_ = RuntimeScreen::TitleMenu;
    dialogue_ = {};
    battle_ = {};
    opening_event_cursor_ = {};
    opening_event_operation_status_ = Xq32OperationStatus::Idle;
    rebuild_menu();
}

void RuntimeScene::begin_dialogue() {
    screen_ = RuntimeScreen::Dialogue;
    selected_index_ = 0;
    items_.clear();
    opening_event_operation_status_ = Xq32OperationStatus::Idle;
    opening_event_cursor_.start(*opening_event_program_, "Main");
    const auto execution = opening_event_cursor_.snapshot();
    dialogue_ = {true, runtime_content_.opening_event_path, "", "",
                 0, execution.instruction_end - execution.instruction_index};
}

RuntimeTransition RuntimeScene::advance_dialogue() {
    if (!dialogue_.active) {
        return {false, RuntimeCommand::None};
    }
    if (opening_event_cursor_.running() && opening_event_program_.has_value()) {
        const auto operation = opening_event_dispatcher_.dispatch(
            opening_event_cursor_, *opening_event_program_);
        opening_event_operation_status_ = operation.status;
        if (operation.status == Xq32OperationStatus::InvalidArguments) {
            return {false, RuntimeCommand::None};
        }
        if (opening_event_cursor_.running() && operation.progressed) {
            dialogue_.line_index = opening_event_cursor_.snapshot().instruction_index -
                (opening_event_cursor_.snapshot().instruction_end - dialogue_.line_count);
            return {true, RuntimeCommand::AdvanceDialogue};
        }
    }
    return_to_menu();
    return {true, RuntimeCommand::AdvanceDialogue};
}

void RuntimeScene::begin_battle() {
    battle_ = {};
    battle_.definition_loaded = battle_content_.ready;
    battle_.configured_command_count =
        static_cast<std::uint32_t>(battle_content_.command_catalog.empty()
                                       ? battle_content_.command_ids.size()
                                       : battle_content_.command_catalog.size());
    if (!battle_content_.ready) {
        return;
    }
    screen_ = RuntimeScreen::Battle;
    items_.clear();
    if (!battle_content_.battle_sequence_ids.empty()) {
        battle_.definition_id = battle_content_.battle_sequence_ids.front();
    }
    battle_.active = battle_.configured_command_count != 0;
    battle_.result = "catalog_only";
}

RuntimeTransition RuntimeScene::resolve_battle_action(RuntimeInput input) {
    if (!battle_.active) {
        return {false, RuntimeCommand::None};
    }
    if (input == RuntimeInput::Run || input == RuntimeInput::Back) {
        return_to_menu();
        return {true, RuntimeCommand::None};
    }
    if (input != RuntimeInput::Confirm && input != RuntimeInput::Attack &&
        input != RuntimeInput::Skill) {
        return {false, RuntimeCommand::None};
    }
    const auto command_count = battle_.configured_command_count;
    if (command_count == 0) {
        return {false, RuntimeCommand::None};
    }
    std::size_t command_index = input == RuntimeInput::Skill ? 1u : 0u;
    if (command_index >= command_count) {
        command_index = 0;
    }
    if (!battle_content_.command_catalog.empty()) {
        battle_.last_command_id = battle_content_.command_catalog[command_index].id;
    } else {
        battle_.last_command_id = battle_content_.command_ids[command_index];
    }
    return {true, RuntimeCommand::None};
}

void RuntimeScene::advance(double delta_seconds) {
    if (screen_ != RuntimeScreen::World || !world_.active || delta_seconds <= 0.0) {
        return;
    }
    constexpr double kMaximumDeltaSeconds = 0.25;
    const auto clamped = std::min(delta_seconds, kMaximumDeltaSeconds);
    world_.simulation_seconds += clamped;
    world_.camera_x = world_.player_x;
    world_.camera_y = world_.player_y;
    ++world_.frame;
}

std::optional<RuntimeSaveState> RuntimeScene::save_state() const {
    // An indexed map and a structural XQ32 trace are not a playable save.
    return std::nullopt;
}

bool RuntimeScene::restore_state(const RuntimeSaveState& state) {
    (void)state;
    return false;
}

void RuntimeScene::select_next(int direction) {
    if (items_.empty()) {
        return;
    }
    const auto count = items_.size();
    for (std::size_t attempts = 0; attempts < count; ++attempts) {
        if (direction > 0) {
            selected_index_ = (selected_index_ + 1) % count;
        } else {
            selected_index_ = selected_index_ == 0 ? count - 1 : selected_index_ - 1;
        }
        if (items_[selected_index_].enabled) {
            return;
        }
    }
}

RuntimeTransition RuntimeScene::handle(RuntimeInput input) {
    if (screen_ == RuntimeScreen::Import) {
        if (input == RuntimeInput::Confirm) {
            return {true, RuntimeCommand::RequestAuthorizedImport};
        }
        return {false, RuntimeCommand::None};
    }

    if (screen_ == RuntimeScreen::Dialogue) {
        if (input == RuntimeInput::Confirm) {
            return advance_dialogue();
        }
        if (input == RuntimeInput::Back) {
            return_to_menu();
            return {true, RuntimeCommand::None};
        }
        return {false, RuntimeCommand::None};
    }

    if (screen_ == RuntimeScreen::Battle) {
        return resolve_battle_action(input);
    }

    if (screen_ == RuntimeScreen::World) {
        return_to_menu();
        return {true, RuntimeCommand::None};
    }

    switch (input) {
        case RuntimeInput::MoveNext:
        case RuntimeInput::MoveDown:
        case RuntimeInput::MoveRight:
            select_next(1);
            return {true, RuntimeCommand::None};
        case RuntimeInput::MovePrevious:
        case RuntimeInput::MoveUp:
        case RuntimeInput::MoveLeft:
            select_next(-1);
            return {true, RuntimeCommand::None};
        case RuntimeInput::Confirm:
            return select_menu_item(selected_index_);
        case RuntimeInput::Back:
            return {false, RuntimeCommand::None};
        case RuntimeInput::Attack:
        case RuntimeInput::Skill:
        case RuntimeInput::Run:
            return {false, RuntimeCommand::None};
    }
    return {false, RuntimeCommand::None};
}

RuntimeTransition RuntimeScene::select_menu_item(std::size_t index) {
    if (screen_ != RuntimeScreen::TitleMenu || index >= items_.size() ||
        !items_[index].enabled) {
        return {false, RuntimeCommand::None};
    }
    selected_index_ = index;
    if (items_[index].id == "inspect_opening") {
        begin_dialogue();
        return {true, RuntimeCommand::InspectOpening};
    }
    if (items_[index].id == "battle_catalog") {
        begin_battle();
        return {true, RuntimeCommand::OpenBattleCatalog};
    }
    return {false, RuntimeCommand::None};
}

RuntimeSnapshot RuntimeScene::snapshot() const {
    RuntimeSnapshot result;
    result.screen = screen_;
    result.presentation = mobile_yw1_smartphone_profile();
    result.selected_index = selected_index_;
    result.items = items_;
    result.initial_content_ready = initial_content_ready_;
    result.initial_content_files = initial_content_files_;
    result.scene_resources = scene_resources_;
    result.runtime_content = runtime_content_;
    result.battle_content = battle_content_;
    result.opening_event_program = opening_event_program_;
    result.opening_event_execution = opening_event_cursor_.snapshot();
    result.opening_event_operation_status = opening_event_operation_status_;
    result.background_width = background_width_;
    result.background_height = background_height_;
    result.background_format = background_format_;
    result.background_rgba = background_rgba_;
    result.dialogue = dialogue_;
    result.world = world_;
    result.battle = battle_;
    result.audio_cue = audio_cue_;
    result.audio_sequence = audio_sequence_;
    if (screen_ == RuntimeScreen::Import) {
        result.status_key = boot_.state == RuntimeBootState::ImportIncomplete
                                ? "import.incomplete"
                                : "import.required";
    } else if (screen_ == RuntimeScreen::Dialogue) {
        result.status_key = "runtime.opening_inspector";
    } else if (screen_ == RuntimeScreen::Battle) {
        result.status_key = "runtime.battle_catalog";
    } else if (!initial_content_ready_) {
        result.status_key = "runtime.content_missing";
    } else {
        result.status_key = "runtime.title_menu";
    }
    return result;
}

std::string runtime_command_name(RuntimeCommand command) {
    switch (command) {
        case RuntimeCommand::RequestAuthorizedImport:
            return "request_authorized_import";
        case RuntimeCommand::StartNewGame:
            return "start_new_game";
        case RuntimeCommand::AdvanceDialogue:
            return "advance_dialogue";
        case RuntimeCommand::StartBattle:
            return "start_battle";
        case RuntimeCommand::BattleWon:
            return "battle_won";
        case RuntimeCommand::BattleEscaped:
            return "battle_escaped";
        case RuntimeCommand::BattleDefeated:
            return "battle_defeated";
        case RuntimeCommand::ContinueGame:
            return "continue_game";
        case RuntimeCommand::OpenSettings:
            return "open_settings";
        case RuntimeCommand::InspectOpening:
            return "inspect_opening";
        case RuntimeCommand::OpenBattleCatalog:
            return "open_battle_catalog";
        case RuntimeCommand::None:
            return "none";
    }
    return "none";
}

namespace {

const char* screen_name(RuntimeScreen screen) {
    switch (screen) {
        case RuntimeScreen::Import:
            return "import";
        case RuntimeScreen::TitleMenu:
            return "title_menu";
        case RuntimeScreen::Dialogue:
            return "dialogue";
        case RuntimeScreen::World:
            return "world";
        case RuntimeScreen::Battle:
            return "battle";
    }
    return "import";
}

}  // namespace

std::string serialize_runtime_snapshot(const RuntimeSnapshot& snapshot) {
    std::string result = "screen=";
    result += screen_name(snapshot.screen);
    result += "\nselected=" + std::to_string(snapshot.selected_index) + "\nstatus=" +
              snapshot.status_key + "\n";
    result += "presentation_id=" + snapshot.presentation.id + "\n";
    result += "presentation_reference=" + snapshot.presentation.reference_style + "\n";
    result += "presentation_scene_height_percent=" +
              std::to_string(snapshot.presentation.scene_height_percent) + "\n";
    result += "presentation_dialogue_top_percent=" +
              std::to_string(snapshot.presentation.dialogue_top_percent) + "\n";
    result += "presentation_portrait_preferred=" +
              std::string(snapshot.presentation.portrait_preferred ? "1\n" : "0\n");
    result += "presentation_touch_controls=" +
              std::string(snapshot.presentation.touch_controls ? "1\n" : "0\n");
    result += "content_ready=" + std::string(snapshot.initial_content_ready ? "1\n" : "0\n");
    result += "content_files=" + std::to_string(snapshot.initial_content_files) + "\n";
    result += "title_sequence=" + snapshot.runtime_content.title_sequence_path + "\n";
    result += "main_menu_sequence=" + snapshot.runtime_content.main_menu_sequence_path + "\n";
    result += "opening_event=" + snapshot.runtime_content.opening_event_path + "\n";
    result += "battle_config=" + snapshot.runtime_content.battle_config_path + "\n";
    result += "battle_rule=" + snapshot.runtime_content.battle_rule_path + "\n";
    result += "battle_command=" + snapshot.runtime_content.battle_command_path + "\n";
    result += "battle_view_config=" +
              snapshot.runtime_content.battle_view_config_path + "\n";
    result += "battle_ai=" + snapshot.runtime_content.battle_ai_path + "\n";
    result += "map_sound_config=" +
              snapshot.runtime_content.map_sound_config_path + "\n";
    result += "opening_event_sound_config=" +
              snapshot.runtime_content.opening_event_sound_config_path + "\n";
    result += "battle_presentation_ready=" +
              std::string(snapshot.runtime_content.has_battle_presentation() ? "1\n" : "0\n");
    result += "audio_definition_ready=" +
              std::string(snapshot.runtime_content.has_audio_definition() ? "1\n" : "0\n");
    result += "battle_content_ready=" +
              std::string(snapshot.battle_content.ready ? "1\n" : "0\n");
    result += "battle_config_entries=" +
              std::to_string(snapshot.battle_content.battle_config_entries) + "\n";
    result += "battle_rule_entries=" +
              std::to_string(snapshot.battle_content.battle_rule_entries) + "\n";
    result += "battle_command_entries=" +
              std::to_string(snapshot.battle_content.battle_command_entries) + "\n";
    result += "battle_view_entries=" +
              std::to_string(snapshot.battle_content.battle_view_entries) + "\n";
    result += "battle_ai_entries=" +
              std::to_string(snapshot.battle_content.battle_ai_entries) + "\n";
    result += "battle_sequence_catalog_records=" +
              std::to_string(snapshot.battle_content.battle_sequence_catalog.size()) + "\n";
    result += "battle_command_catalog_records=" +
              std::to_string(snapshot.battle_content.command_catalog.size()) + "\n";
    result += "sound_catalog_records=" +
              std::to_string(snapshot.battle_content.sound_catalog.size()) + "\n";
    const auto append_catalog = [&result](const char* key,
                                           const std::vector<BattleCatalogRecord>& catalog) {
        const auto limit = std::min<std::size_t>(catalog.size(), 32);
        for (std::size_t index = 0; index < limit; ++index) {
            const auto& record = catalog[index];
            result += std::string(key) + "_record=" + record.id + "|" +
                      std::to_string(record.entry_index) + "|" +
                      std::to_string(record.hash) + "|" +
                      std::to_string(record.raw_values.size()) + "\n";
        }
    };
    append_catalog("battle_sequence", snapshot.battle_content.battle_sequence_catalog);
    append_catalog("battle_command", snapshot.battle_content.command_catalog);
    append_catalog("sound", snapshot.battle_content.sound_catalog);
    const auto sequence_limit = std::min<std::size_t>(
        snapshot.battle_content.battle_sequence_ids.size(), 32);
    for (std::size_t index = 0; index < sequence_limit; ++index) {
        result += "battle_sequence_id=" +
                  snapshot.battle_content.battle_sequence_ids[index] + "\n";
    }
    const auto command_limit = std::min<std::size_t>(
        snapshot.battle_content.command_ids.size(), 32);
    for (std::size_t index = 0; index < command_limit; ++index) {
        result += "battle_command_id=" +
                  snapshot.battle_content.command_ids[index] + "\n";
    }
    const auto sound_limit = std::min<std::size_t>(
        snapshot.battle_content.sound_ids.size(), 32);
    for (std::size_t index = 0; index < sound_limit; ++index) {
        result += "sound_id=" + snapshot.battle_content.sound_ids[index] + "\n";
    }
    for (const auto& path : snapshot.runtime_content.battle_texture_paths) {
        result += "battle_texture=" + path + "\n";
    }
    result += "dialogue_active=" + std::string(snapshot.dialogue.active ? "1\n" : "0\n");
    result += "event_id=" + snapshot.dialogue.event_id + "\n";
    if (snapshot.opening_event_program.has_value()) {
        const auto& program = *snapshot.opening_event_program;
        result += "event_functions=" + std::to_string(program.header.function_count) + "\n";
        result += "event_jumps=" + std::to_string(program.header.jump_count) + "\n";
        result += "event_instructions=" + std::to_string(program.header.instruction_count) + "\n";
        result += "event_arguments=" + std::to_string(program.header.argument_count) + "\n";
        result += "event_globals=" + std::to_string(program.header.global_variable_count) + "\n";
        result += "event_decoded_functions=" +
                  std::to_string(program.function_records.size()) + "\n";
        result += "event_decoded_jumps=" + std::to_string(program.jump_records.size()) + "\n";
        result += "event_decoded_instructions=" +
                  std::to_string(program.instruction_records.size()) + "\n";
        result += "event_decoded_arguments=" +
                  std::to_string(program.argument_records.size()) + "\n";
        const auto trace_count = std::min<std::size_t>(program.instruction_records.size(), 8);
        for (std::size_t index = 0; index < trace_count; ++index) {
            result += "event_instruction_row=" + std::to_string(index);
            for (const auto word : program.instruction_records[index].words) {
                result += "|" + std::to_string(word);
            }
            result += "\n";
        }
        for (const auto& symbol : program.symbols) {
            result += "event_symbol=" + symbol + "\n";
        }
    }
    result += "event_exec_state=" +
              std::string(xq32_execution_state_name(snapshot.opening_event_execution.state)) +
              "\n";
    result += "event_exec_operation_status=" +
              std::string(xq32_operation_status_name(snapshot.opening_event_operation_status)) +
              "\n";
    result += "event_exec_entry=" + snapshot.opening_event_execution.entry_symbol + "\n";
    result += "event_exec_instruction=" +
              std::to_string(snapshot.opening_event_execution.instruction_index) + "\n";
    result += "event_exec_end=" +
              std::to_string(snapshot.opening_event_execution.instruction_end) + "\n";
    if (snapshot.opening_event_execution.has_instruction) {
        const auto& instruction = snapshot.opening_event_execution.instruction;
        result += "event_exec_operation=" + std::to_string(instruction.operation_token) + "\n";
        result += "event_exec_operation_id=" + std::to_string(instruction.operation_id) + "\n";
        result += "event_exec_operation_group=" +
                  std::to_string(instruction.operation_group) + "\n";
        result += "event_exec_argument_index=" +
                  std::to_string(instruction.argument_index) + "\n";
        result += "event_exec_argument_count=" +
                  std::to_string(instruction.argument_count) + "\n";
        result += "event_exec_argument_span_valid=" +
                  std::string(instruction.argument_span_valid ? "1\n" : "0\n");
    }
    result += "speaker=" + snapshot.dialogue.speaker + "\n";
    result += "text_key=" + snapshot.dialogue.text_key + "\n";
    result += "line_index=" + std::to_string(snapshot.dialogue.line_index) + "\n";
    result += "line_count=" + std::to_string(snapshot.dialogue.line_count) + "\n";
    result += "world_active=" + std::string(snapshot.world.active ? "1\n" : "0\n");
    result += "world_map=" + snapshot.world.map_resource + "\n";
    result += "player_x=" + std::to_string(snapshot.world.player_x) + "\n";
    result += "player_y=" + std::to_string(snapshot.world.player_y) + "\n";
    result += "camera_x=" + std::to_string(snapshot.world.camera_x) + "\n";
    result += "camera_y=" + std::to_string(snapshot.world.camera_y) + "\n";
    result += "simulation_seconds=" + std::to_string(snapshot.world.simulation_seconds) + "\n";
    result += "frame=" + std::to_string(snapshot.world.frame) + "\n";
    result += "battle_active=" + std::string(snapshot.battle.active ? "1\n" : "0\n");
    result += "battle_definition_loaded=" +
              std::string(snapshot.battle.definition_loaded ? "1\n" : "0\n");
    result += "battle_definition_id=" + snapshot.battle.definition_id + "\n";
    result += "battle_configured_command_count=" +
              std::to_string(snapshot.battle.configured_command_count) + "\n";
    result += "battle_last_command_id=" + snapshot.battle.last_command_id + "\n";
    result += "enemy_id=" + snapshot.battle.enemy_id + "\n";
    result += "player_hp=" + std::to_string(snapshot.battle.player_hp) + "\n";
    result += "player_max_hp=" + std::to_string(snapshot.battle.player_max_hp) + "\n";
    result += "enemy_hp=" + std::to_string(snapshot.battle.enemy_hp) + "\n";
    result += "enemy_max_hp=" + std::to_string(snapshot.battle.enemy_max_hp) + "\n";
    result += "battle_turn=" + std::to_string(snapshot.battle.turn) + "\n";
    result += "battle_result=" + snapshot.battle.result + "\n";
    result += "audio_cue=" + snapshot.audio_cue + "\n";
    result += "audio_sequence=" + std::to_string(snapshot.audio_sequence) + "\n";
    for (const auto& resource : snapshot.scene_resources) {
        result += "scene_resource=" +
                  std::string(initial_scene_resource_role_name(resource.role)) + "|" +
                  resource.path + "|" + std::to_string(resource.size) + "|" +
                  std::to_string(resource.offset) + "|" +
                  std::to_string(static_cast<unsigned int>(resource.kind)) + "|" +
                  std::to_string(resource.child_count) + "\n";
        if (resource.texture_width != 0 && resource.texture_height != 0) {
            result += "scene_texture=" + resource.path + "|" +
                      std::to_string(resource.texture_width) + "|" +
                      std::to_string(resource.texture_height) + "|" +
                      std::to_string(static_cast<unsigned int>(resource.texture_format)) +
                      "|" + std::to_string(resource.texture_rgba_bytes) + "\n";
        }
        if (!resource.mesh_preview_positions.empty()) {
            result += "scene_mesh=" + resource.path + "|" +
                      std::to_string(resource.mesh_vertex_count) + "|" +
                      std::to_string(resource.mesh_triangle_index_count) + "|" +
                      std::to_string(resource.mesh_material_hash) + "\n";
            const auto position_count = resource.mesh_preview_positions.size() / 3;
            for (std::size_t index = 0; index < position_count; ++index) {
                result += "scene_mesh_vertex=" + resource.path + "|" +
                          std::to_string(resource.mesh_preview_positions[index * 3]) + "|" +
                          std::to_string(resource.mesh_preview_positions[index * 3 + 1]) + "|" +
                          std::to_string(resource.mesh_preview_positions[index * 3 + 2]) + "\n";
            }
            for (const auto index : resource.mesh_preview_indices) {
                result += "scene_mesh_index=" + resource.path + "|" +
                          std::to_string(index) + "\n";
            }
        }
    }
    for (const auto& item : snapshot.items) {
        result += "item=" + item.id + "|" + item.label_key + "|" +
                  (item.enabled ? "1" : "0") + "\n";
    }
    return result;
}

std::string serialize_runtime_response(const RuntimeTransition& transition,
                                       const RuntimeSnapshot& snapshot) {
    return "command=" + runtime_command_name(transition.command) + "\nhandled=" +
           (transition.handled ? "1\n" : "0\n") + serialize_runtime_snapshot(snapshot);
}

}  // namespace ykw2::core
