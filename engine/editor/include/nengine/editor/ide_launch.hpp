#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace nengine::editor {
enum class IdeChoice { Associated, VisualStudio, Rider };
struct IdeLaunch {
    std::filesystem::path executable{};
    std::filesystem::path solution{};
    std::filesystem::path working_directory{};
    std::wstring parameters{};
    bool use_file_association{true};
};
// Platform-neutral launch planning. An explicit executable overrides PATH
// detection, must exist and must be an absolute trusted local executable.
std::optional<IdeLaunch> prepare_ide_launch(
    std::filesystem::path solution,
    IdeChoice preferred,
    const std::filesystem::path& executable_override={},
    std::string* error=nullptr);
} // namespace nengine::editor
