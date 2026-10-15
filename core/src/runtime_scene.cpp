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
    opening_event_program_.reset();
    opening_event_cursor_ = {};
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
    items_ = {
        {"new_game", "menu.new_game", initial_content_ready_},
        {"continue", "menu.continue", false},
        {"settings", "menu.settings", true},
    };
}

void RuntimeScene::start(const RuntimeBootInfo& boot, const InitialContentInfo& content) {
    start(boot, true, content.archive_files);
    scene_resources_ = content.scene_resources;
    runtime_content_ = content.runtime_content;
    opening_event_program_ = content.opening_event_program;
    background_width_ = content.background_width;
    background_height_ = content.background_height;
    background_format_ = content.background_format;
    background_rgba_ = content.background_rgba;
}

bool RuntimeScene::selected_item_enabled() const {
    return screen_ == RuntimeScreen::TitleMenu && selected_index_ < items_.size() &&
           items_[selected_index_].enabled;
}

void RuntimeScene::enter_world() {
    screen_ = RuntimeScreen::World;
    selected_index_ = 0;
    items_.clear();
    world_ = {};
    world_.active = true;
    for (const auto& resource : scene_resources_) {
        if (resource.role == InitialSceneResourceRole::Map) {
            world_.map_resource = resource.path;
            break;
        }
    }
}

void RuntimeScene::trigger_audio(std::string cue) {
    audio_cue_ = std::move(cue);
    ++audio_sequence_;
}

void RuntimeScene::begin_dialogue() {
    screen_ = RuntimeScreen::Dialogue;
    selected_index_ = 0;
    items_.clear();
    std::size_t instruction_count = 0;
    std::string entry_symbol = "XQ32";
    if (opening_event_program_.has_value()) {
        const auto& program = *opening_event_program_;
        if (opening_event_cursor_.start(program, "Main")) {
            const auto execution = opening_event_cursor_.snapshot();
            instruction_count = execution.instruction_end - execution.instruction_index;
            if (!execution.entry_symbol.empty()) {
                entry_symbol = execution.entry_symbol;
            }
        } else {
            instruction_count = program.instruction_records.size();
        }
        if (instruction_count == 0) {
            // Keep synthetic/partially decoded programs navigable while the
            // ABI recovery is in progress. Real YW2 programs use the decoded
            // row count above; a metadata-only fixture gets one checkpoint.
            instruction_count = 1;
        }
        if (!program.symbols.empty()) {
            entry_symbol = program.symbols.front();
        }
    }
    if (instruction_count == 0) {
        instruction_count = 1;
    }
    dialogue_ = {true, runtime_content_.opening_event_path, entry_symbol,
                 "xq32.step.1", 0, instruction_count};
    trigger_audio("event_begin");
}

RuntimeTransition RuntimeScene::advance_dialogue() {
    if (!dialogue_.active) {
        return {false, RuntimeCommand::None};
    }
    if (opening_event_cursor_.running() && opening_event_cursor_.advance()) {
        ++dialogue_.line_index;
        dialogue_.text_key = "xq32.step." + std::to_string(dialogue_.line_index + 1);
        trigger_audio("event_advance");
        return {true, RuntimeCommand::AdvanceDialogue};
    }
    if (!opening_event_cursor_.running() && dialogue_.line_count != 0 &&
        dialogue_.line_index + 1 < dialogue_.line_count) {
        ++dialogue_.line_index;
        dialogue_.text_key = "xq32.step." + std::to_string(dialogue_.line_index + 1);
        trigger_audio("event_advance");
        return {true, RuntimeCommand::AdvanceDialogue};
    }
    dialogue_.active = false;
    enter_world();
    trigger_audio("event_end");
    return {true, RuntimeCommand::AdvanceDialogue};
}

void RuntimeScene::begin_battle() {
    // Battle data is identified by the manifest, but a parser/interpreter for
    // the YW2 rule and encounter data must exist before a battle is entered.
    // Do not substitute an invented enemy, HP values or attacks here.
    battle_ = {};
}

RuntimeTransition RuntimeScene::resolve_battle_action(RuntimeInput input) {
    if (!battle_.active) {
        return {false, RuntimeCommand::None};
    }
    if (input == RuntimeInput::Run || input == RuntimeInput::Back) {
        battle_.active = false;
        battle_.result = "escaped";
        screen_ = RuntimeScreen::World;
        world_.active = true;
        trigger_audio("run");
        return {true, RuntimeCommand::BattleEscaped};
    }
    if (input != RuntimeInput::Confirm && input != RuntimeInput::Attack &&
        input != RuntimeInput::Skill) {
        return {false, RuntimeCommand::None};
    }
    const int damage = input == RuntimeInput::Skill ? 35 : 18;
    battle_.enemy_hp = std::max(0, battle_.enemy_hp - damage);
    ++battle_.turn;
    trigger_audio(input == RuntimeInput::Skill ? "skill" : "attack");
    if (battle_.enemy_hp == 0) {
        battle_.active = false;
        battle_.result = "won";
        screen_ = RuntimeScreen::World;
        world_.active = true;
        trigger_audio("victory");
        return {true, RuntimeCommand::BattleWon};
    }
    battle_.player_hp = std::max(0, battle_.player_hp - (input == RuntimeInput::Skill ? 8 : 5));
    if (battle_.player_hp == 0) {
        battle_.active = false;
        battle_.result = "defeated";
        screen_ = RuntimeScreen::World;
        world_.active = true;
        trigger_audio("defeat");
        return {true, RuntimeCommand::BattleDefeated};
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
    if (screen_ != RuntimeScreen::World || !world_.active || world_.map_resource.empty()) {
        return std::nullopt;
    }
    return RuntimeSaveState{true, world_.map_resource, world_.player_x, world_.player_y,
                            world_.simulation_seconds, world_.frame};
}

bool RuntimeScene::restore_state(const RuntimeSaveState& state) {
    if (!initial_content_ready_ || !state.active || state.map_resource.empty()) {
        return false;
    }
    const auto resource = std::find_if(
        scene_resources_.begin(), scene_resources_.end(), [&state](const auto& candidate) {
            return candidate.role == InitialSceneResourceRole::Map &&
                   candidate.path == state.map_resource;
        });
    if (resource == scene_resources_.end()) {
        return false;
    }
    screen_ = RuntimeScreen::World;
    selected_index_ = 0;
    items_.clear();
    world_.active = true;
    world_.map_resource = state.map_resource;
    world_.player_x = state.player_x;
    world_.player_y = state.player_y;
    world_.camera_x = state.player_x;
    world_.camera_y = state.player_y;
    world_.simulation_seconds = state.simulation_seconds;
    world_.frame = state.frame;
    return true;
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
        if (input == RuntimeInput::Confirm || input == RuntimeInput::Attack) {
            return advance_dialogue();
        }
        if (input == RuntimeInput::Back) {
            enter_world();
            return {true, RuntimeCommand::None};
        }
        return {false, RuntimeCommand::None};
    }

    if (screen_ == RuntimeScreen::Battle) {
        return resolve_battle_action(input);
    }

    if (screen_ == RuntimeScreen::World) {
        constexpr float kStep = 1.0f;
        switch (input) {
            case RuntimeInput::MoveNext:
            case RuntimeInput::MoveRight:
                world_.player_x += kStep;
                advance(1.0 / 60.0);
                return {true, RuntimeCommand::None};
            case RuntimeInput::MovePrevious:
            case RuntimeInput::MoveLeft:
                world_.player_x -= kStep;
                advance(1.0 / 60.0);
                return {true, RuntimeCommand::None};
            case RuntimeInput::MoveUp:
                world_.player_y -= kStep;
                advance(1.0 / 60.0);
                return {true, RuntimeCommand::None};
            case RuntimeInput::MoveDown:
                world_.player_y += kStep;
                advance(1.0 / 60.0);
                return {true, RuntimeCommand::None};
            case RuntimeInput::Back:
                screen_ = RuntimeScreen::TitleMenu;
                world_.active = false;
                items_ = {
                    {"new_game", "menu.new_game", initial_content_ready_},
                    {"continue", "menu.continue", false},
                    {"settings", "menu.settings", true},
                };
                selected_index_ = 0;
                return {true, RuntimeCommand::None};
            case RuntimeInput::Confirm:
                if (!runtime_content_.has_battle_definition()) {
                    return {false, RuntimeCommand::None};
                }
                begin_battle();
                return {false, RuntimeCommand::None};
            case RuntimeInput::Attack:
            case RuntimeInput::Skill:
            case RuntimeInput::Run:
                return {false, RuntimeCommand::None};
        }
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
            if (!selected_item_enabled()) {
                return {false, RuntimeCommand::None};
            }
            switch (selected_index_) {
                case 0:
                    begin_dialogue();
                    return {true, RuntimeCommand::StartNewGame};
                case 2:
                    return {true, RuntimeCommand::OpenSettings};
                default:
                    return {false, RuntimeCommand::None};
            }
        case RuntimeInput::Back:
            return {false, RuntimeCommand::None};
        case RuntimeInput::Attack:
        case RuntimeInput::Skill:
        case RuntimeInput::Run:
            return {false, RuntimeCommand::None};
    }
    return {false, RuntimeCommand::None};
}

RuntimeSnapshot RuntimeScene::snapshot() const {
    RuntimeSnapshot result;
    result.screen = screen_;
    result.selected_index = selected_index_;
    result.items = items_;
    result.initial_content_ready = initial_content_ready_;
    result.initial_content_files = initial_content_files_;
    result.scene_resources = scene_resources_;
    result.runtime_content = runtime_content_;
    result.opening_event_program = opening_event_program_;
    result.opening_event_execution = opening_event_cursor_.snapshot();
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
    } else if (screen_ == RuntimeScreen::World) {
        result.status_key = "runtime.world";
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
    result += "content_ready=" + std::string(snapshot.initial_content_ready ? "1\n" : "0\n");
    result += "content_files=" + std::to_string(snapshot.initial_content_files) + "\n";
    result += "title_sequence=" + snapshot.runtime_content.title_sequence_path + "\n";
    result += "main_menu_sequence=" + snapshot.runtime_content.main_menu_sequence_path + "\n";
    result += "opening_event=" + snapshot.runtime_content.opening_event_path + "\n";
    result += "battle_config=" + snapshot.runtime_content.battle_config_path + "\n";
    result += "battle_rule=" + snapshot.runtime_content.battle_rule_path + "\n";
    result += "battle_command=" + snapshot.runtime_content.battle_command_path + "\n";
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
    result += "event_exec_entry=" + snapshot.opening_event_execution.entry_symbol + "\n";
    result += "event_exec_instruction=" +
              std::to_string(snapshot.opening_event_execution.instruction_index) + "\n";
    result += "event_exec_end=" +
              std::to_string(snapshot.opening_event_execution.instruction_end) + "\n";
    if (snapshot.opening_event_execution.has_instruction) {
        const auto& instruction = snapshot.opening_event_execution.instruction;
        result += "event_exec_operation=" + std::to_string(instruction.operation_token) + "\n";
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
