#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nengine::scripting {

struct DotnetHostDiscovery {
    std::filesystem::path dotnet_root{};
    std::filesystem::path hostfxr_path{};
    std::string version{};

    bool valid() const noexcept {
        return
            !dotnet_root.empty() &&
            !hostfxr_path.empty() &&
            !version.empty();
    }
};

// Locate the newest host/fxr/<version>/hostfxr library below explicit_root.
// When explicit_root is empty, environment + conventional installation roots
// are searched in deterministic priority order.
std::optional<DotnetHostDiscovery>
discover_dotnet_host(
    const std::filesystem::path& explicit_root = {},
    std::string* error = nullptr);

struct DotnetRuntimeConfig {
    std::string target_framework{"net8.0"};
    std::string framework_name{
        "Microsoft.NETCore.App"};
    std::string framework_version{"8.0.0"};
    std::string roll_forward{"LatestMajor"};
};

bool write_dotnet_runtime_config(
    const std::filesystem::path& path,
    const DotnetRuntimeConfig& config,
    std::string* error = nullptr);

// Thin hostfxr runtime loader. No compile-time .NET SDK dependency is needed:
// all hostfxr entry points are resolved dynamically.
class DotnetHost {
public:
    DotnetHost() = default;
    ~DotnetHost();

    DotnetHost(
        const DotnetHost&) = delete;
    DotnetHost& operator=(
        const DotnetHost&) = delete;

    DotnetHost(
        DotnetHost&& other) noexcept;
    DotnetHost& operator=(
        DotnetHost&& other) noexcept;

    bool initialize(
        const std::filesystem::path& hostfxr_path,
        const std::filesystem::path& runtime_config,
        std::string* error = nullptr);

    void* load_unmanaged_entry(
        const std::filesystem::path& assembly_path,
        std::string_view assembly_qualified_type,
        std::string_view method_name,
        std::string* error = nullptr) const;

    void shutdown() noexcept;

    bool ready() const noexcept {
        return library_ != nullptr &&
            runtime_context_ != nullptr &&
            load_assembly_ != nullptr;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    void* library_{nullptr};
    void* runtime_context_{nullptr};
    void* load_assembly_{nullptr};
    void* close_hostfxr_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::scripting
