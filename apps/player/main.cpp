#include <filesystem>
#include <iostream>
#include "nengine/player/runtime.hpp"

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
    std::cout << "Player scene loaded: " << runtime.manifest().title
              << " (" << runtime.entity_count() << " entities)\n";
    return 0;
}
