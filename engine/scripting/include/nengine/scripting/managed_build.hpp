#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nengine::scripting {

struct ManagedBuildConfig {
    std::filesystem::path project_path{};
    std::filesystem::path output_directory{};
    std::filesystem::path dotnet_executable{};
    std::string configuration{"Debug"};
    bool restore{true};
};

struct ManagedBuildPlan {
    std::filesystem::path executable{};
    std::vector<std::string> arguments{};
    std::filesystem::path assembly_path{};
    std::filesystem::path pdb_path{};

    bool valid() const noexcept {
        return
            !executable.empty() &&
            !arguments.empty() &&
            !assembly_path.empty();
    }
};

struct ManagedBuildResult {
    bool success{false};
    int exit_code{-1};
    ManagedBuildPlan plan{};
    std::string message{};
};

std::optional<std::filesystem::path>
discover_dotnet_executable(
    const std::filesystem::path& explicit_path = {});

std::optional<ManagedBuildPlan>
make_managed_build_plan(
    const ManagedBuildConfig& config,
    std::string* error = nullptr);

ManagedBuildResult run_managed_build(
    const ManagedBuildConfig& config);

} // namespace nengine::scripting
