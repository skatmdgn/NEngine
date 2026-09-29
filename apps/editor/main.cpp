#include <iostream>
#include <memory>

#include "nengine/editor/editor_model.hpp"

int main() {
    nengine::editor::EditorModel editor;
    auto& world = editor.world();

    const auto camera = world.create("Main Camera");
    const auto cube = world.create("Cube");
    world.set_parent(cube, camera);
    editor.selection().set(cube);

    editor.commands().execute(world, std::make_unique<nengine::editor::RenameEntityCommand>(cube, "Demo Cube"));
    editor.play_session().play(world);

    std::cout << "NEngine Editor model bootstrap 0.2.0-dev\n";
    std::cout << "Edit World: " << world.size() << " objects\n";
    std::cout << "Runtime World: " << editor.play_session().runtime_world()->size() << " objects\n";
    std::cout << "Next: native Windows host + dockable editor presentation\n";
    return 0;
}
