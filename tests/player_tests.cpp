#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "nengine/player/bundle.hpp"

int main() {
    using namespace nengine::player;
    BundleManifest manifest;
    manifest.title = "Demo";
    std::stringstream stream;
    if (!write_bundle(stream, manifest)) return 1;
    BundleManifest restored;
    if (!read_bundle(stream, restored) ||
        restored.title != manifest.title ||
        restored.scene != manifest.scene) return 2;
    restored.scene = "../outside.nscene";
    if (validate_bundle(restored)) return 3;
    restored = manifest;
    restored.width = 0;
    if (validate_bundle(restored)) return 4;
    const auto root = std::filesystem::temp_directory_path() /
        "nengine_player_bundle_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Content");
    { std::ofstream file(root / "Content" / "Main.nscene"); file << "fixture"; }
    std::string error;
    if (!resolve_bundle_scene(root, manifest, &error)) return 5;
    if (resolve_bundle_scene(root / "missing", manifest, &error)) return 6;
    std::filesystem::remove_all(root);
    std::cout << "player manifest tests passed\n";
    return 0;
}
