#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/initial_content.hpp"
#include "ykw2/core/mobile_presentation.hpp"
#include "ykw2/core/runtime_bootstrap.hpp"
#include "ykw2/core/runtime_save.hpp"
#include "ykw2/core/xq32_dispatch.hpp"
#include "ykw2/core/xq32_execution.hpp"

namespace ykw2::core {

enum class RuntimeScreen {
    Import,
    TitleMenu,
    Dialogue,
    World,
    Battle,
};

enum class RuntimeInput {
    MoveNext,
    MovePrevious,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,
    Attack,
    Skill,
    Run,
    Back,
};

enum class RuntimeCommand {
    None,
    RequestAuthorizedImport,
    StartNewGame,
    AdvanceDialogue,
    StartBattle,
    BattleWon,
    BattleEscaped,
    BattleDefeated,
    ContinueGame,
    OpenSettings,
};

struct RuntimeMenuItem {
    std::string id;
    // UI adapters localize this stable key; the core contains no platform UI.
    std::string label_key;
    bool enabled = true;
};

struct RuntimeWorldSnapshot {
    bool active = false;
    std::string map_resource;
    float player_x = 0.0f;
    float player_y = 0.0f;
    float camera_x = 0.0f;
    float camera_y = 0.0f;
    double simulation_seconds = 0.0;
    std::uint64_t frame = 0;
};

struct RuntimeDialogueSnapshot {
    bool active = false;
    std::string event_id;
    std::string speaker;
    std::string text_key;
    std::size_t line_index = 0;
    std::size_t line_count = 0;
};

struct RuntimeBattleSnapshot {
    bool active = false;
    bool definition_loaded = false;
    std::string definition_id;
    std::uint32_t configured_command_count = 0;
    std::string enemy_id;
    int player_hp = 0;
    int player_max_hp = 0;
    int enemy_hp = 0;
    int enemy_max_hp = 0;
    std::uint32_t turn = 0;
    std::string result;
};

struct RuntimeSnapshot {
    RuntimeScreen screen = RuntimeScreen::Import;
    std::size_t selected_index = 0;
    std::vector<RuntimeMenuItem> items;
    std::string status_key;
    MobilePresentationProfile presentation;
    bool initial_content_ready = false;
    std::uint64_t initial_content_files = 0;
    std::vector<InitialSceneResource> scene_resources;
    RuntimeContentManifest runtime_content;
    InitialBattleContent battle_content;
    std::optional<Xq32ProgramInfo> opening_event_program;
    Xq32ExecutionSnapshot opening_event_execution;
    Xq32OperationStatus opening_event_operation_status = Xq32OperationStatus::Idle;
    std::uint16_t background_width = 0;
    std::uint16_t background_height = 0;
    std::uint8_t background_format = 0;
    std::vector<std::uint8_t> background_rgba;
    RuntimeDialogueSnapshot dialogue;
    RuntimeWorldSnapshot world;
    RuntimeBattleSnapshot battle;
    std::string audio_cue;
    std::uint64_t audio_sequence = 0;
};

struct RuntimeTransition {
    bool handled = false;
    RuntimeCommand command = RuntimeCommand::None;
};

// Platform-neutral first scene. It deliberately exposes commands instead of
// drawing anything, so Android and iOS can use their own rendering/input APIs.
class RuntimeScene {
public:
    RuntimeScene() = default;

    void start(const RuntimeBootInfo& boot);
    void start(const RuntimeBootInfo& boot, bool initial_content_ready,
               std::uint64_t initial_content_files);
    void start(const RuntimeBootInfo& boot, const InitialContentInfo& content);
    RuntimeTransition handle(RuntimeInput input);
    void advance(double delta_seconds);
    std::optional<RuntimeSaveState> save_state() const;
    bool restore_state(const RuntimeSaveState& state);
    RuntimeSnapshot snapshot() const;
    const std::vector<std::uint8_t>& background_rgba() const { return background_rgba_; }
    std::uint16_t background_width() const { return background_width_; }
    std::uint16_t background_height() const { return background_height_; }

private:
    void begin_dialogue();
    RuntimeTransition advance_dialogue();
    void begin_battle();
    RuntimeTransition resolve_battle_action(RuntimeInput input);
    void trigger_audio(std::string cue);
    void enter_world();
    void select_next(int direction);
    bool selected_item_enabled() const;

    RuntimeBootInfo boot_{};
    bool initial_content_ready_ = false;
    std::uint64_t initial_content_files_ = 0;
    std::vector<InitialSceneResource> scene_resources_;
    RuntimeContentManifest runtime_content_;
    InitialBattleContent battle_content_;
    std::optional<Xq32ProgramInfo> opening_event_program_;
    Xq32ExecutionCursor opening_event_cursor_;
    Xq32OperationDispatcher opening_event_dispatcher_;
    Xq32OperationStatus opening_event_operation_status_ = Xq32OperationStatus::Idle;
    std::uint16_t background_width_ = 0;
    std::uint16_t background_height_ = 0;
    std::uint8_t background_format_ = 0;
    std::vector<std::uint8_t> background_rgba_;
    RuntimeDialogueSnapshot dialogue_;
    RuntimeWorldSnapshot world_;
    RuntimeBattleSnapshot battle_;
    std::string audio_cue_;
    std::uint64_t audio_sequence_ = 0;
    RuntimeScreen screen_ = RuntimeScreen::Import;
    std::size_t selected_index_ = 0;
    std::vector<RuntimeMenuItem> items_;
};

std::string serialize_runtime_snapshot(const RuntimeSnapshot& snapshot);
std::string serialize_runtime_response(const RuntimeTransition& transition,
                                       const RuntimeSnapshot& snapshot);
std::string runtime_command_name(RuntimeCommand command);

}  // namespace ykw2::core
