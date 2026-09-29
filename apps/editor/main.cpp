#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "nengine/editor/editor_model.hpp"
#include "nengine/platform/window.hpp"

int main() {
    nengine::editor::EditorModel editor;
    auto& world = editor.world();

    const auto camera = world.create("Main Camera");
    const auto cube = world.create("Cube");
    world.set_parent(cube, camera);
    editor.selection().set(cube);
    editor.commands().execute(world, std::make_unique<nengine::editor::RenameEntityCommand>(cube, "Demo Cube"));

    nengine::platform::Window window({"NEngine Editor 0.2.0-dev", 1440, 900, true});
    if (!window.open()) {
        std::cerr << "Failed to create NEngine editor window.\n";
        return 1;
    }

#if defined(_WIN32)
    while (window.poll_events()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    std::cout << "NEngine Editor model bootstrap 0.2.0-dev\n";
    std::cout << "Edit World: " << world.size() << " objects\n";
    std::cout << "Native Windows host is compiled and exercised by Windows CI.\n";
    window.close();
#endif
    return 0;
}
