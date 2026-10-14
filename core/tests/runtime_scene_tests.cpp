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
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::StartNewGame);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    snapshot = scene.snapshot();
    // The unavailable Continue item is skipped by navigation.
    assert(snapshot.selected_index == 2);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::OpenSettings);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    assert(scene.snapshot().selected_index == 0);
}

}  // namespace

int main() {
    test_import_screen();
    test_incomplete_import_screen();
    test_title_menu_navigation();
    return 0;
}
