#include "ykw2/core/runtime_scene.hpp"

#include <algorithm>

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
    result.initial_content_ready = initial_content_ready_;
    result.initial_content_files = initial_content_files_;
    result.scene_resources = scene_resources_;
    if (screen_ == RuntimeScreen::Import) {
        result.status_key = boot_.state == RuntimeBootState::ImportIncomplete
                                ? "import.incomplete"
                                : "import.required";
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
    return screen == RuntimeScreen::TitleMenu ? "title_menu" : "import";
}

}  // namespace

std::string serialize_runtime_snapshot(const RuntimeSnapshot& snapshot) {
    std::string result = "screen=";
    result += screen_name(snapshot.screen);
    result += "\nselected=" + std::to_string(snapshot.selected_index) + "\nstatus=" +
              snapshot.status_key + "\n";
    result += "content_ready=" + std::string(snapshot.initial_content_ready ? "1\n" : "0\n");
    result += "content_files=" + std::to_string(snapshot.initial_content_files) + "\n";
    for (const auto& resource : snapshot.scene_resources) {
        result += "scene_resource=" +
                  std::string(initial_scene_resource_role_name(resource.role)) + "|" +
                  resource.path + "|" + std::to_string(resource.size) + "|" +
                  std::to_string(resource.offset) + "|" +
                  std::to_string(static_cast<unsigned int>(resource.kind)) + "|" +
                  std::to_string(resource.child_count) + "\n";
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
