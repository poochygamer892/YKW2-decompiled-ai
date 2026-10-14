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
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find("screen=title_menu\n") == 0);
    assert(ykw2::core::serialize_runtime_response(
               {true, ykw2::core::RuntimeCommand::StartNewGame}, snapshot)
               .find("command=start_new_game\n") == 0);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    snapshot = scene.snapshot();
    // The unavailable Continue item is skipped by navigation.
    assert(snapshot.selected_index == 2);
    assert(scene.handle(ykw2::core::RuntimeInput::Confirm).command ==
           ykw2::core::RuntimeCommand::OpenSettings);

    scene.handle(ykw2::core::RuntimeInput::MoveNext);
    assert(scene.snapshot().selected_index == 0);
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

    ykw2::core::RuntimeScene scene;
    scene.start(boot(ykw2::core::RuntimeBootState::Ready), content);
    const auto snapshot = scene.snapshot();
    assert(snapshot.scene_resources.size() == 1);
    assert(snapshot.scene_resources[0].path == "data/map/e100g002/e100g002.xc");
    assert(ykw2::core::serialize_runtime_snapshot(snapshot).find(
               "scene_resource=map|data/map/e100g002/e100g002.xc|30|100|2|12\n") !=
           std::string::npos);
}

}  // namespace

int main() {
    test_import_screen();
    test_incomplete_import_screen();
    test_title_menu_navigation();
    test_title_menu_waits_for_initial_content();
    test_title_menu_exposes_scene_resource_plan();
    return 0;
}
