#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nengine::scripting {

struct NuGetPackageReference {
    std::string id{};
    std::string version{};
};

struct ManagedProjectConfig {
    std::string project_name{"GameScripts"};
    std::filesystem::path project_root{};
    std::string target_framework{"net8.0"};
    std::string runtime_framework_version{"8.0.0"};
    std::vector<NuGetPackageReference> packages{};
};

struct ManagedProjectOutput {
    std::filesystem::path solution_path{};
    std::filesystem::path project_path{};
    std::filesystem::path api_stub_path{};
    std::filesystem::path bridge_path{};
    std::filesystem::path runtime_config_path{};
};

class ManagedProjectGenerator {
public:
    static bool generate(
        const ManagedProjectConfig& config,
        ManagedProjectOutput& output,
        std::string* error = nullptr);

    static std::vector<NuGetPackageReference>
    load_package_manifest(
        const std::filesystem::path& path,
        std::string* error = nullptr);
};

} // namespace nengine::scripting
