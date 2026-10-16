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
    std::cout << "player manifest tests passed\n";
    return 0;
}
