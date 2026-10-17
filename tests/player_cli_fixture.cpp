#include <filesystem>
#include <iostream>
#include <string>
#include "nengine/core/scene.hpp"
#include "nengine/editor/project_manifest.hpp"
#include "nengine/input/action_map_io.hpp"

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path project = std::filesystem::u8path(argv[1]);
    std::error_code ec;
    std::filesystem::create_directories(project / "Assets" / "Scenes", ec);
    if (ec) return 3;
    nengine::editor::ProjectManifest manifest;
    manifest.name = "Player CI Fixture";
    std::string error;
    if (!nengine::editor::ProjectManifestSerializer::save(
            manifest, project / "NEngine.nproject", &error)) {
        std::cerr << error << '\n'; return 4;
    }
    nengine::core::World world;
    world.create("Camera");
    world.create("Player");
    const auto scene = nengine::core::SceneSerializer::capture(world, "Main");
    if (!nengine::core::SceneSerializer::save_file(
            scene, project / manifest.startup_scene, &error)) {
        std::cerr << error << '\n'; return 5;
    }
    nengine::input::ActionMap actions;
    if (!actions.bind("Jump", nengine::input::Key::Space) ||
        !actions.bind("Fire", nengine::input::Key::MouseLeft) ||
        !nengine::input::save_action_map_file(
            project / "Input.ninput", actions, &error)) return 6;
    return 0;
}
