#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace nengine::player {

struct ExportSettings {
    std::filesystem::path project_root{};
    std::filesystem::path startup_scene{};
    std::filesystem::path destination{};
    // Host-platform standalone Player binary staged with the game data.
    // Leave empty for data-only bundle generation in tests/tools.
    std::filesystem::path player_executable{};
    bool copy_supported_assets{true};
    std::filesystem::path managed_directory{};
    std::string managed_assembly_name{"GameScripts"};
    std::string title{"NEngine Game"};
    std::string game_version{"0.1.0"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
};

struct ExportReport {
    std::filesystem::path bundle_root{};
    std::size_t scene_objects{0};
    std::size_t copied_assets{0};
    std::size_t cooked_textures{0};
    std::size_t cooked_materials{0};
    std::size_t cooked_models{0};
    std::size_t managed_files{0};
    std::size_t generated_materials{0};
    std::size_t generated_textures{0};
};

// Produces a validated DATA bundle. The Player executable will be bundled
// separately once the runner path is known; not yet an APK/AAB exporter.
bool export_player_data(const ExportSettings& settings,
                        ExportReport& report, std::string* error = nullptr);
}
