#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/scripting/components.hpp"
#include "nengine/scripting/dotnet_host.hpp"
#include "nengine/scripting/managed_build.hpp"
#include "nengine/scripting/managed_project.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/scripting/script_system.hpp"

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
            output.api_project_path),
        "dedicated NEngine API csproj generated");

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
            "NEngine.API.csproj") !=
                std::string::npos &&
        project.find(
            "<Compile Include=\"NEngine.API.cs\"") ==
                std::string::npos &&
        project.find(
            "GenerateRuntimeConfigurationFiles") !=
                std::string::npos,
        "gameplay csproj targets hostable net8 and references dedicated NEngine API project");

    const auto api_project =
        read_all(
            output.api_project_path);

    check(
        api_project.find(
            "<AssemblyName>NEngine.API</AssemblyName>") !=
                std::string::npos &&
        api_project.find(
            "<Compile Include=\"NEngine.API.cs\"") !=
                std::string::npos,
        "dedicated NEngine API project builds only the generated authoring API assembly");

    const auto solution =
        read_all(
            output.solution_path);

    check(
        solution.find(
            "\"NEngine.API\"") !=
                std::string::npos &&
        solution.find(
            "NEngine.API.csproj") !=
                std::string::npos,
        "solution includes dedicated NEngine API project");

    const auto api =
        read_all(output.api_stub_path);

    check(
        api.find("class Behaviour") !=
            std::string::npos &&
        api.find("class GameObject") !=
            std::string::npos &&
        api.find("class Transform") !=
            std::string::npos &&
        api.find("GetInstanceID") !=
            std::string::npos &&
        api.find("GetComponent<T>") !=
            std::string::npos &&
        api.find(
            "InternalsVisibleTo(\"GameScripts\")") !=
                std::string::npos,
        "managed API stub exposes familiar authoring types and grants bridge assembly internal access");


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
            "AbiVersion = 4") !=
                std::string::npos &&
        bridge.find(
            "SetGameObjectState") !=
                std::string::npos &&
        bridge.find(
            "GetGameObjectState") !=
                std::string::npos &&
        bridge.find(
            "CopyGameObjectNameUtf8") !=
                std::string::npos,
        "managed bridge exposes lifecycle Transform and GameObject ABI v4 entries");

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

    const auto explicit_dotnet =
        root /
#if defined(_WIN32)
        "fake-dotnet.exe";
#else
        "fake-dotnet-command";
#endif

    {
        std::ofstream fake(
            explicit_dotnet,
            std::ios::binary |
                std::ios::trunc);

        fake << "placeholder";
    }

    const auto explicit_discovered =
        discover_dotnet_executable(
            explicit_dotnet);

    check(
        explicit_discovered &&
        *explicit_discovered ==
            std::filesystem::absolute(
                explicit_dotnet),
        "managed build discovery accepts explicit dotnet executable path");

    ManagedBuildConfig plan_config;
    plan_config.project_path =
        output.project_path;
    plan_config.output_directory =
        root /
        "Library" /
        "ManagedBuild" /
        "Debug";
    plan_config.dotnet_executable =
        explicit_dotnet;
    plan_config.configuration =
        "Debug";
    plan_config.restore = false;

    std::string build_error;

    const auto build_plan =
        make_managed_build_plan(
            plan_config,
            &build_error);

    check(
        build_plan &&
        build_plan->valid() &&
        build_plan->assembly_path ==
            plan_config.output_directory /
            "GameScripts.dll" &&
        build_plan->pdb_path ==
            plan_config.output_directory /
            "GameScripts.pdb" &&
        std::find(
            build_plan->arguments.begin(),
            build_plan->arguments.end(),
            "--no-restore") !=
                build_plan->arguments.end(),
        "managed build plan selects deterministic DLL/PDB output and no-restore flag");

    const auto real_dotnet =
        discover_dotnet_executable();

    if (real_dotnet) {
        const auto integration_root =
            root /
            "integration";

        std::filesystem::create_directories(
            integration_root /
            "Assets" /
            "Scripts");

        {
            std::ofstream script(
                integration_root /
                    "Assets" /
                    "Scripts" /
                    "Example.cs",
                std::ios::binary |
                    std::ios::trunc);

            script
                << "using NEngine;\n"
                << "public class Example : Behaviour {\n"
                << "    public int starts;\n"
                << "    public int updates;\n"
                << "    private void Start() { starts++; gameObject.name = \"Managed Renamed\"; }\n"
                << "    private void Update() { updates++; var t = GetComponent<Transform>(); if (t == null) throw new System.Exception(\"Transform missing\"); t.localPosition = t.localPosition + new Vector3(1, 2, 3); }\n"
                << "}\n"
                << "public class DeactivateOnce : Behaviour {\n"
                << "    private void Update() { gameObject.SetActive(false); }\n"
                << "}\n";
        }

        ManagedProjectConfig
            integration_project;

        integration_project.project_name =
            "IntegrationScripts";
        integration_project.project_root =
            integration_root;

        ManagedProjectOutput
            integration_output;

        std::string integration_error;

        const bool generated =
            ManagedProjectGenerator::generate(
                integration_project,
                integration_output,
                &integration_error);

        check(
            generated,
            "managed build integration project generation succeeds");

        if (generated) {
            ManagedBuildConfig
                integration_build;

            integration_build.project_path =
                integration_output.project_path;
            integration_build.output_directory =
                integration_root /
                "Library" /
                "ManagedBuild" /
                "Debug";
            integration_build.dotnet_executable =
                *real_dotnet;
            integration_build.configuration =
                "Debug";

            const auto build =
                run_managed_build(
                    integration_build);

            check(
                build.success &&
                build.exit_code == 0 &&
                std::filesystem::exists(
                    build.plan.assembly_path) &&
                build.plan.assembly_path
                    .filename() ==
                    "IntegrationScripts.dll" &&
                std::filesystem::exists(
                    integration_build
                        .output_directory /
                    "NEngine.API.dll"),
                "real dotnet SDK builds gameplay assembly plus dedicated NEngine API dependency into deterministic output");


            if (build.success) {
                std::string runtime_error;

                const auto runtime =
                    discover_dotnet_host(
                        {},
                        &runtime_error);

                check(
                    runtime &&
                    runtime->valid(),
                    "real dotnet runtime host is discoverable after managed build");

                if (runtime) {
                    ManagedRuntime
                        managed_runtime;

                    const bool initialized =
                        managed_runtime.initialize(
                            runtime->hostfxr_path,
                            integration_output
                                .runtime_config_path,
                            build.plan
                                .assembly_path,
                            "IntegrationScripts");

                    check(
                        initialized,
                        "ManagedRuntime initializes generated gameplay assembly lifecycle bridge");

                    if (initialized) {
                        check(
                            managed_runtime.instance_count() == 0,
                            "managed lifecycle starts with no Behaviour instances");

                        const auto behaviour =
                            managed_runtime.create_behaviour(
                                "Example");

                        check(
                            behaviour.valid() &&
                            managed_runtime.instance_count() == 1,
                            "managed lifecycle creates Behaviour instance by C# type name");

                        if (behaviour.valid()) {
                            const auto runtime_entity =
                                nengine::core::Entity::make(
                                    7u,
                                    3u);

                            check(
                                managed_runtime.set_game_object(
                                    behaviour,
                                    runtime_entity,
                                    "Runtime Object",
                                    false),
                                "managed runtime pushes native GameObject identity name and active state");

                            nengine::core::Entity
                                pulled_entity =
                                    nengine::core::Entity::invalid();

                            std::string
                                pulled_name;

                            bool pulled_active = true;

                            check(
                                managed_runtime.get_game_object(
                                    behaviour,
                                    pulled_entity,
                                    pulled_name,
                                    pulled_active) &&
                                pulled_entity ==
                                    runtime_entity &&
                                pulled_name ==
                                    "Runtime Object" &&
                                !pulled_active,
                                "managed runtime pulls GameObject identity name and active state without loss");

                            check(
                                managed_runtime.start(
                                    behaviour),
                                "managed lifecycle invokes Start");

                            check(
                                managed_runtime.update(
                                    behaviour,
                                    1.0f / 60.0f),
                                "managed lifecycle invokes Update with frame delta");

                            check(
                                managed_runtime.destroy(
                                    behaviour) &&
                                managed_runtime.instance_count() == 0,
                                "managed lifecycle invokes OnDestroy path and releases instance handle");
                        }

                        ManagedScriptSystem
                            script_system;

                        script_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            script_world;

                        const auto script_entity =
                            script_world.create(
                                "Managed Example");

                        auto* script_component =
                            script_world.add_component<
                                ScriptBehaviour>(
                                    script_entity,
                                    script_behaviour_type());

                        check(
                            script_component != nullptr,
                            "ScriptBehaviour component attaches to World entity for lifecycle test");

                        if (script_component) {
                            script_component->type_name =
                                "Example";

                            auto* native_transform =
                                script_world.transform(
                                    script_entity);

                            if (native_transform) {
                                native_transform->local_position = {
                                    5.0f,
                                    0.0f,
                                    0.0f
                                };
                            }

                            std::string system_error;

                            const auto first_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                first_tick.created == 1u &&
                                first_tick.started == 1u &&
                                first_tick.updated == 1u &&
                                first_tick.unresolved == 0u &&
                                script_system.instance_count() == 1u &&
                                managed_runtime.instance_count() == 1,
                                "ScriptBehaviour first World tick creates starts and updates managed instance");

                            check(
                                native_transform &&
                                native_transform->local_position.x == 6.0f &&
                                native_transform->local_position.y == 2.0f &&
                                native_transform->local_position.z == 3.0f,
                                "managed Update writes localPosition back into native World Transform");

                            check(
                                script_world.name(
                                    script_entity) ==
                                    "Managed Renamed",
                                "managed Start writes GameObject.name back into native World");

                            const auto second_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 30.0f,
                                    &system_error);

                            check(
                                second_tick.created == 0u &&
                                second_tick.started == 0u &&
                                second_tick.updated == 1u &&
                                second_tick.unresolved == 0u,
                                "ScriptBehaviour subsequent World tick reuses managed instance and only updates");

                            check(
                                native_transform &&
                                native_transform->local_position.x == 7.0f &&
                                native_transform->local_position.y == 4.0f &&
                                native_transform->local_position.z == 6.0f,
                                "managed Transform synchronization accumulates across World ticks");

                            script_component->enabled =
                                false;

                            const auto disabled_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                disabled_tick.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "disabling ScriptBehaviour destroys managed instance");

                            script_component->enabled =
                                true;

                            const auto reenabled_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                reenabled_tick.created == 1u &&
                                reenabled_tick.started == 1u &&
                                reenabled_tick.updated == 1u &&
                                managed_runtime.instance_count() == 1,
                                "reenabling ScriptBehaviour recreates and restarts managed instance");

                            check(
                                native_transform &&
                                native_transform->local_position.x == 8.0f &&
                                native_transform->local_position.y == 6.0f &&
                                native_transform->local_position.z == 9.0f,
                                "recreated managed Behaviour receives current native Transform before Update");

                            script_world.destroy(
                                script_entity);

                            const auto destroyed_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                destroyed_tick.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "destroying ScriptBehaviour entity invokes managed OnDestroy and releases handle");

                            const auto active_entity =
                                script_world.create(
                                    "Deactivate Target");

                            auto* active_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        active_entity,
                                        script_behaviour_type());

                            if (active_script) {
                                active_script->type_name =
                                    "DeactivateOnce";
                            }

                            const auto deactivate_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                active_script &&
                                deactivate_tick.created == 1u &&
                                deactivate_tick.started == 1u &&
                                deactivate_tick.updated == 1u &&
                                deactivate_tick.unresolved == 0u &&
                                !script_world.active(
                                    active_entity) &&
                                script_system.instance_count() == 1u,
                                "managed GameObject.SetActive(false) writes back into native World after Update");

                            const auto inactive_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                inactive_tick.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "inactive native GameObject stops ScriptBehaviour and releases current managed instance");

                        }
                    }
                }
            }
        }
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
