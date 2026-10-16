#include <filesystem>
#include <iostream>
#include <string>

#include "nengine/editor/project_manifest.hpp"
#include "nengine/player/exporter.hpp"
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: NEnginePlayerBuild <project-root> <destination>"
                     " [player-executable]\n";
        return 2;
    }
    const std::filesystem::path project{argv[1]};
    const std::filesystem::path destination{argv[2]};
    nengine::editor::ProjectManifest project_manifest;
    std::string error;
    if (!nengine::editor::ProjectManifestSerializer::load(
            project / "NEngine.nproject", project_manifest, &error)) {
        std::cerr << "Project manifest invalid: " << error << "\n";
        return 3;
    }
#if defined(_WIN32)
    if (!project_manifest.target_windows) {
        std::cerr << "Project has Windows export disabled\n";
        return 4;
    }
#endif
    nengine::player::ExportSettings settings;
    settings.project_root = project;
    settings.startup_scene = project_manifest.startup_scene;
    settings.destination = destination;
    settings.title = project_manifest.name;
    if (argc == 4) {
        settings.player_executable = std::filesystem::path(argv[3]);
    } else {
#if defined(_WIN32)
        // Shell PATH lookup need not contain an absolute argv[0].
        // Resolve the builder's actual module location instead.
        wchar_t module_path[32768]{};
        const DWORD count = GetModuleFileNameW(
            nullptr, module_path, static_cast<DWORD>(32768));
        if (!count || count >= 32768) {
            std::cerr << "Cannot locate Player builder executable\n";
            return 6;
        }
        auto executable = std::filesystem::path(module_path).parent_path();
        executable /= "NEnginePlayer.exe";
#else
        auto executable = std::filesystem::absolute(
            std::filesystem::path(argv[0])).parent_path();
        executable /= "NEnginePlayer";
#endif
        settings.player_executable = std::move(executable);
    }
    nengine::player::ExportReport result;
    if (!nengine::player::export_player_data(settings, result, &error)) {
        std::cerr << "Player export failed: " << error << "\n";
        return 5;
    }
    std::cout << "Player bundle ready: " << result.bundle_root.string()
              << " (" << result.scene_objects << " scene objects)\n";
    return 0;
}
