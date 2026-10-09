#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/scripting/dotnet_host.hpp"
#include "nengine/scripting/managed_project.hpp"

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}
}

int main() {
    using namespace nengine::scripting;

    const auto stamp =
        std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();

    const auto root =
        std::filesystem::temp_directory_path() /
        ("nengine_scripting_test_" +
         std::to_string(stamp));

    std::filesystem::create_directories(
        root / "Assets" / "Scripts");

    {
        std::ofstream script(
            root / "Assets" / "Scripts" / "Player.cs",
            std::ios::binary | std::ios::trunc);

        script
            << "using NEngine;\n"
            << "public class Player : Behaviour {}\n";
    }

    ManagedProjectConfig config;
    config.project_name = "GameScripts";
    config.project_root = root;
    config.packages.push_back({
        "Newtonsoft.Json",
        "13.0.3"
    });

    ManagedProjectOutput output;
    std::string error;

    check(
        ManagedProjectGenerator::generate(
            config,
            output,
            &error),
        "managed project generation succeeds");

    check(
        std::filesystem::exists(
            output.solution_path),
        "solution file generated");

    check(
        std::filesystem::exists(
            output.project_path),
        "csproj generated");

    check(
        std::filesystem::exists(
            output.api_stub_path),
        "NEngine managed API stub generated");


    check(
        std::filesystem::exists(
            output.bridge_path),
        "NEngine managed native ABI bridge generated");

    check(
        std::filesystem::exists(
            output.runtime_config_path),
        "managed runtimeconfig generated");

    const auto project =
        read_all(output.project_path);

    check(
        project.find(
            "../../Assets/Scripts/**/*.cs") !=
            std::string::npos,
        "csproj includes project script glob");

    check(
        project.find(
            "Newtonsoft.Json") !=
            std::string::npos &&
        project.find(
            "13.0.3") !=
            std::string::npos,
        "csproj includes NuGet PackageReference");


    check(
        project.find(
            "<TargetFramework>net8.0</TargetFramework>") !=
                std::string::npos &&
        project.find(
            "NEngine.ManagedBridge.cs") !=
                std::string::npos &&
        project.find(
            "GenerateRuntimeConfigurationFiles") !=
                std::string::npos,
        "csproj targets hostable net8 and compiles managed bridge");

    const auto api =
        read_all(output.api_stub_path);

    check(
        api.find("class Behaviour") !=
            std::string::npos &&
        api.find("class GameObject") !=
            std::string::npos &&
        api.find("class Transform") !=
            std::string::npos,
        "managed API stub exposes familiar authoring types");


    const auto bridge =
        read_all(
            output.bridge_path);

    check(
        bridge.find(
            "[UnmanagedCallersOnly]") !=
                std::string::npos &&
        bridge.find(
            "GetAbiVersion") !=
                std::string::npos &&
        bridge.find(
            "AbiVersion = 1") !=
                std::string::npos,
        "managed bridge exposes stable unmanaged ABI version entry");

    const auto runtime_config =
        read_all(
            output.runtime_config_path);

    check(
        runtime_config.find(
            "\"tfm\": \"net8.0\"") !=
                std::string::npos &&
        runtime_config.find(
            "\"version\": \"8.0.0\"") !=
                std::string::npos &&
        runtime_config.find(
            "\"rollForward\": \"LatestMajor\"") !=
                std::string::npos,
        "runtimeconfig selects net8 framework with forward-compatible roll policy");

    const auto fake_dotnet =
        root /
        "fake-dotnet";

#if defined(_WIN32)
    const std::filesystem::path
        hostfxr_name =
            "hostfxr.dll";
#elif defined(__APPLE__)
    const std::filesystem::path
        hostfxr_name =
            "libhostfxr.dylib";
#else
    const std::filesystem::path
        hostfxr_name =
            "libhostfxr.so";
#endif

    const auto fxr_8 =
        fake_dotnet /
        "host" /
        "fxr" /
        "8.0.12" /
        hostfxr_name;

    const auto fxr_10 =
        fake_dotnet /
        "host" /
        "fxr" /
        "10.0.1" /
        hostfxr_name;

    std::filesystem::create_directories(
        fxr_8.parent_path());
    std::filesystem::create_directories(
        fxr_10.parent_path());

    {
        std::ofstream output_file(
            fxr_8,
            std::ios::binary |
                std::ios::trunc);
        output_file << "fake";
    }

    {
        std::ofstream output_file(
            fxr_10,
            std::ios::binary |
                std::ios::trunc);
        output_file << "fake";
    }

    std::string host_error;

    const auto discovered =
        discover_dotnet_host(
            fake_dotnet,
            &host_error);

    check(
        discovered &&
        discovered->valid() &&
        discovered->version ==
            "10.0.1" &&
        discovered->hostfxr_path ==
            fxr_10,
        "hostfxr discovery selects newest numeric runtime version");

    const auto invalid_root =
        root /
        "missing-dotnet";

    host_error.clear();

    check(
        !discover_dotnet_host(
            invalid_root,
            &host_error) &&
        host_error.find(
            "hostfxr") !=
                std::string::npos,
        "explicit missing dotnet root produces diagnostic");

    DotnetRuntimeConfig custom_runtime;
    custom_runtime.target_framework =
        "net9.0";
    custom_runtime.framework_version =
        "9.0.2";
    custom_runtime.roll_forward =
        "LatestMinor";

    const auto custom_runtime_path =
        root /
        "runtime" /
        "Custom.runtimeconfig.json";

    check(
        write_dotnet_runtime_config(
            custom_runtime_path,
            custom_runtime,
            &host_error),
        "standalone runtimeconfig writer succeeds");

    const auto custom_runtime_text =
        read_all(
            custom_runtime_path);

    check(
        custom_runtime_text.find(
            "\"tfm\": \"net9.0\"") !=
                std::string::npos &&
        custom_runtime_text.find(
            "\"version\": \"9.0.2\"") !=
                std::string::npos &&
        custom_runtime_text.find(
            "\"rollForward\": \"LatestMinor\"") !=
                std::string::npos,
        "standalone runtimeconfig writer preserves configured framework policy");

    const auto packages_path =
        root /
        "Packages" /
        "managed-packages.txt";

    std::filesystem::create_directories(
        packages_path.parent_path());

    {
        std::ofstream packages(
            packages_path,
            std::ios::binary | std::ios::trunc);

        packages
            << "# id version\n"
            << "Newtonsoft.Json 13.0.3\n"
            << "Example.Package 1.2.3\n";
    }

    const auto loaded =
        ManagedProjectGenerator::
            load_package_manifest(
                packages_path,
                &error);

    check(
        loaded.size() == 2,
        "managed package manifest parses package rows");

    check(
        loaded[0].id == "Newtonsoft.Json" &&
        loaded[0].version == "13.0.3",
        "managed package manifest preserves version");

    std::error_code cleanup;
    std::filesystem::remove_all(
        root,
        cleanup);

    if (failures == 0) {
        std::cout << "NEngineScriptingTests: PASS\n";
        return EXIT_SUCCESS;
    }

    std::cerr
        << "NEngineScriptingTests: "
        << failures
        << " failure(s)\n";

    return EXIT_FAILURE;
}
