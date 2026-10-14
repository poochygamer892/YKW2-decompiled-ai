#include "ykw2/core/runtime_scene.hpp"

#include <algorithm>

namespace ykw2::core {

void RuntimeScene::start(const RuntimeBootInfo& boot) {
    boot_ = boot;
    selected_index_ = 0;
    items_.clear();
    if (boot.state != RuntimeBootState::Ready) {
        screen_ = RuntimeScreen::Import;
        return;
    }

    screen_ = RuntimeScreen::TitleMenu;
    items_ = {
        {"new_game", "menu.new_game", true},
        {"continue", "menu.continue", false},
        {"settings", "menu.settings", true},
    };
}

bool RuntimeScene::selected_item_enabled() const {
    return screen_ == RuntimeScreen::TitleMenu && selected_index_ < items_.size() &&
           items_[selected_index_].enabled;
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

    switch (input) {
        case RuntimeInput::MoveNext:
            select_next(1);
            return {true, RuntimeCommand::None};
        case RuntimeInput::MovePrevious:
            select_next(-1);
            return {true, RuntimeCommand::None};
        case RuntimeInput::Confirm:
            if (!selected_item_enabled()) {
                return {false, RuntimeCommand::None};
            }
            switch (selected_index_) {
                case 0:
                    return {true, RuntimeCommand::StartNewGame};
                case 2:
                    return {true, RuntimeCommand::OpenSettings};
                default:
                    return {false, RuntimeCommand::None};
            }
        case RuntimeInput::Back:
            return {false, RuntimeCommand::None};
    }
    return {false, RuntimeCommand::None};
}

RuntimeSnapshot RuntimeScene::snapshot() const {
    RuntimeSnapshot result;
    result.screen = screen_;
    result.selected_index = selected_index_;
    result.items = items_;
    if (screen_ == RuntimeScreen::Import) {
        result.status_key = boot_.state == RuntimeBootState::ImportIncomplete
                                ? "import.incomplete"
                                : "import.required";
    } else {
        result.status_key = "runtime.title_menu";
    }
    return result;
}

}  // namespace ykw2::core
