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
    background_width_ = 0;
    background_height_ = 0;
    background_format_ = 0;
    background_rgba_.clear();
    world_ = {};
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
                    enter_world();
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
    result.background_width = background_width_;
    result.background_height = background_height_;
    result.background_format = background_format_;
    result.background_rgba = background_rgba_;
    result.world = world_;
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
        case RuntimeScreen::World:
            return "world";
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
    result += "world_active=" + std::string(snapshot.world.active ? "1\n" : "0\n");
    result += "world_map=" + snapshot.world.map_resource + "\n";
    result += "player_x=" + std::to_string(snapshot.world.player_x) + "\n";
    result += "player_y=" + std::to_string(snapshot.world.player_y) + "\n";
    result += "camera_x=" + std::to_string(snapshot.world.camera_x) + "\n";
    result += "camera_y=" + std::to_string(snapshot.world.camera_y) + "\n";
    result += "simulation_seconds=" + std::to_string(snapshot.world.simulation_seconds) + "\n";
    result += "frame=" + std::to_string(snapshot.world.frame) + "\n";
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
