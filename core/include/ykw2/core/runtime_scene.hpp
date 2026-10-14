#pragma once

#include <cstddef>
#include <string>
#include <vector>

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
    RuntimeTransition handle(RuntimeInput input);
    RuntimeSnapshot snapshot() const;

private:
    void select_next(int direction);
    bool selected_item_enabled() const;

    RuntimeBootInfo boot_{};
    RuntimeScreen screen_ = RuntimeScreen::Import;
    std::size_t selected_index_ = 0;
    std::vector<RuntimeMenuItem> items_;
};

}  // namespace ykw2::core
