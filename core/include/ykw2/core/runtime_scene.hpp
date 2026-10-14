#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ykw2/core/initial_content.hpp"
#include "ykw2/core/runtime_bootstrap.hpp"

namespace ykw2::core {

enum class RuntimeScreen {
    Import,
    TitleMenu,
};

enum class RuntimeInput {
    MoveNext,
    MovePrevious,
    Confirm,
    Back,
};

enum class RuntimeCommand {
    None,
    RequestAuthorizedImport,
    StartNewGame,
    ContinueGame,
    OpenSettings,
};

struct RuntimeMenuItem {
    std::string id;
    // UI adapters localize this stable key; the core contains no platform UI.
    std::string label_key;
    bool enabled = true;
};

struct RuntimeSnapshot {
    RuntimeScreen screen = RuntimeScreen::Import;
    std::size_t selected_index = 0;
    std::vector<RuntimeMenuItem> items;
    std::string status_key;
    bool initial_content_ready = false;
    std::uint64_t initial_content_files = 0;
    std::vector<InitialSceneResource> scene_resources;
    std::uint16_t background_width = 0;
    std::uint16_t background_height = 0;
    std::uint8_t background_format = 0;
    std::vector<std::uint8_t> background_rgba;
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
    RuntimeSnapshot snapshot() const;
    const std::vector<std::uint8_t>& background_rgba() const { return background_rgba_; }
    std::uint16_t background_width() const { return background_width_; }
    std::uint16_t background_height() const { return background_height_; }

private:
    void select_next(int direction);
    bool selected_item_enabled() const;

    RuntimeBootInfo boot_{};
    bool initial_content_ready_ = false;
    std::uint64_t initial_content_files_ = 0;
    std::vector<InitialSceneResource> scene_resources_;
    std::uint16_t background_width_ = 0;
    std::uint16_t background_height_ = 0;
    std::uint8_t background_format_ = 0;
    std::vector<std::uint8_t> background_rgba_;
    RuntimeScreen screen_ = RuntimeScreen::Import;
    std::size_t selected_index_ = 0;
    std::vector<RuntimeMenuItem> items_;
};

std::string serialize_runtime_snapshot(const RuntimeSnapshot& snapshot);
std::string serialize_runtime_response(const RuntimeTransition& transition,
                                       const RuntimeSnapshot& snapshot);
std::string runtime_command_name(RuntimeCommand command);

}  // namespace ykw2::core
