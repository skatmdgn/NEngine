#include <filesystem>
#include <iostream>
#include "nengine/player/runtime.hpp"
#include "nengine/core/build_identity.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: NEnginePlayer <bundle-directory>\n";
        return 2;
    }
    nengine::player::PlayerRuntime runtime;
    std::string error;
    if (!runtime.load(std::filesystem::u8path(argv[1]), &error)) {
        std::cerr << "Player startup failed: " << error << "\n";
        return 1;
    }
    // Portable CI/headless verification. Windows uses its own Vulkan runner.
    if (runtime.pending_script_count())
        std::cerr << "WARNING: " << runtime.pending_script_count()
                  << " ScriptBehaviour component(s) loaded without managed"
                     " gameplay execution; standalone bootstrap is incomplete.\n";
    std::cout << nengine::core::build_identity(runtime.manifest().game_version)
              << " | Player scene loaded: " << runtime.manifest().title
              << " (" << runtime.entity_count() << " entities)\n";
    return 0;
}
