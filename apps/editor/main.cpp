#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "nengine/editor/editor_model.hpp"
#include "nengine/platform/window.hpp"

#if defined(_WIN32)
#include "win32_editor_shell.hpp"
#endif

int main() {
    nengine::editor::EditorModel editor;
    auto& world = editor.world();

    const auto camera = world.create("Main Camera");
    world.transform(camera)->local_position = {0.0f, 4.0f, -8.0f};

    const auto cube = world.create("Cube");
    world.transform(cube)->local_position = {0.0f, 0.0f, 0.0f};

    const auto child = world.create("Child Cube");
    world.transform(child)->local_position = {2.0f, 0.0f, 1.0f};
    world.set_parent(child, cube);

    editor.selection().set(cube);

    nengine::platform::Window window({"NEngine Editor 0.2.2-dev", 1440, 900, true});
    if (!window.open()) {
        std::cerr << "Failed to create NEngine editor window.\n";
        return 1;
    }

#if defined(_WIN32)
    nengine::app::Win32EditorShell shell(editor);
    if (!shell.attach(window.native_handle())) {
        std::cerr << "Failed to attach NEngine editor shell.\n";
        return 2;
    }
    shell.refresh();

    while (window.poll_events()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    std::cout << "NEngine Editor 0.2.2-dev\n";
    std::cout << "Edit World: " << world.size() << " objects\n";
    std::cout << "Win32 editor shell is exercised by Windows CI and Windows runtime QA.\n";
    window.close();
#endif

    return 0;
}
