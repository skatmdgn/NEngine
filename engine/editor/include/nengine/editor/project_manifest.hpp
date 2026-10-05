#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nengine::editor {

struct ProjectManifest {
    static constexpr std::uint32_t current_version = 1;

    std::uint32_t version{current_version};
    std::string name{"NEngineProject"};
    std::filesystem::path startup_scene{"Assets/Scenes/Main.nscene"};
    std::string scripting{"CSharp"};
    bool target_windows{true};
    bool target_android{true};
    std::string android_toolchain{"default"};
};

class ProjectManifestSerializer {
public:
    static bool validate(
        const ProjectManifest& manifest,
        std::string* error = nullptr);

    static bool save(
        const ProjectManifest& manifest,
        const std::filesystem::path& path,
        std::string* error = nullptr);

    static bool load(
        const std::filesystem::path& path,
        ProjectManifest& manifest,
        std::string* error = nullptr);
};

} // namespace nengine::editor
