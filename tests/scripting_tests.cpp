#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/input/input_state.hpp"
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
            output.bridge_project_path),
        "dedicated NEngine Bridge csproj generated");

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
            "NEngine.ManagedBridge.cs") ==
                std::string::npos &&
        project.find(
            "NEngine.API.csproj") !=
                std::string::npos &&
        project.find(
            "NEngine.Bridge.csproj") !=
                std::string::npos &&
        project.find(
            "ReferenceOutputAssembly=\"false\"") !=
                std::string::npos &&
        project.find(
            "<Compile Include=\"NEngine.API.cs\"") ==
                std::string::npos &&
        project.find(
            "GenerateRuntimeConfigurationFiles") !=
                std::string::npos,
        "gameplay csproj compiles only user scripts and references API while building Bridge out-of-band");

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

    const auto bridge_project =
        read_all(
            output.bridge_project_path);

    check(
        bridge_project.find(
            "<AssemblyName>NEngine.Bridge</AssemblyName>") !=
                std::string::npos &&
        bridge_project.find(
            "NEngine.ManagedBridge.cs") !=
                std::string::npos &&
        bridge_project.find(
            "NEngine.API.csproj") !=
                std::string::npos,
        "dedicated managed Bridge project builds stable ABI host against NEngine API");

    const auto solution =
        read_all(
            output.solution_path);

    check(
        solution.find(
            "\"NEngine.API\"") !=
                std::string::npos &&
        solution.find(
            "NEngine.API.csproj") !=
                std::string::npos &&
        solution.find(
            "\"NEngine.Bridge\"") !=
                std::string::npos &&
        solution.find(
            "NEngine.Bridge.csproj") !=
                std::string::npos,
        "solution includes dedicated NEngine API and Bridge projects");

    const auto api =
        read_all(output.api_stub_path);

    check(
        api.find("class Behaviour") !=
            std::string::npos &&
        api.find("class GameObject") !=
            std::string::npos &&
        api.find("GameObject? Find") !=
            std::string::npos &&
        api.find("static void Destroy") !=
            std::string::npos &&
        api.find("class Transform") !=
            std::string::npos &&
        api.find("GetInstanceID") !=
            std::string::npos &&
        api.find("GetComponent<T>") !=
            std::string::npos &&
        api.find("HasComponent<T>") !=
            std::string::npos &&
        api.find("childCount") !=
            std::string::npos &&
        api.find("GetChild") !=
            std::string::npos &&
        api.find("class NativeWorld") !=
            std::string::npos &&
        api.find("enum KeyCode") !=
            std::string::npos &&
        api.find("class Input") !=
            std::string::npos &&
        api.find("GetKeyDown") !=
            std::string::npos &&
        api.find("mousePosition") !=
            std::string::npos &&
        api.find("StartCoroutine") !=
            std::string::npos &&
        api.find("WaitForSeconds") !=
            std::string::npos &&
        api.find("frameCount") !=
            std::string::npos &&
        api.find(
            "InternalsVisibleTo(\"NEngine.Bridge\")") !=
                std::string::npos,
        "managed API stub exposes familiar authoring types and grants stable Bridge assembly internal access");


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
            "AbiVersion = 7") !=
                std::string::npos &&
        bridge.find(
            "GameplayLoadContext") !=
                std::string::npos &&
        bridge.find(
            "LoadGameplayAssembly") !=
                std::string::npos &&
        bridge.find(
            "UnloadGameplayAssembly") !=
                std::string::npos &&
        bridge.find(
            "SetGameObjectState") !=
                std::string::npos &&
        bridge.find(
            "GetGameObjectState") !=
                std::string::npos &&
        bridge.find(
            "CopyGameObjectNameUtf8") !=
                std::string::npos &&
        bridge.find(
            "ConfigureNativeWorldCallbacks") !=
                std::string::npos &&
        bridge.find(
            "ConfigureNativeInputCallbacks") !=
                std::string::npos &&
        bridge.find(
            "AdvanceFrameClock") !=
                std::string::npos &&
        bridge.find(
            "ResetFrameClock") !=
                std::string::npos,
        "managed bridge exposes collectible gameplay lifecycle native World input and frame-clock ABI v7 entries");

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
                << "}\n"
                << "public class HierarchyProbe : Behaviour {\n"
                << "    private void Update() {\n"
                << "        Transform? p = transform.parent;\n"
                << "        if (p == null) throw new System.Exception(\"parent missing\");\n"
                << "        if (!p.gameObject.HasComponent<Transform>()) throw new System.Exception(\"native transform missing\");\n"
                << "        if (p.childCount != 1) throw new System.Exception(\"child count mismatch\");\n"
                << "        Transform c = p.GetChild(0);\n"
                << "        if (c.gameObject.GetInstanceID() != gameObject.GetInstanceID()) throw new System.Exception(\"child identity mismatch\");\n"
                << "        p.gameObject.name = \"Managed Parent\";\n"
                << "        p.localPosition = p.localPosition + new Vector3(2, 0, 0);\n"
                << "        transform.parent = null;\n"
                << "    }\n"
                << "}\n"
                << "public class InputProbe : Behaviour {\n"
                << "    private int frame;\n"
                << "    private void Update() {\n"
                << "        if (frame == 0) {\n"
                << "            if (!Input.GetKey(KeyCode.A) || !Input.GetKeyDown(KeyCode.A) || Input.GetKeyUp(KeyCode.A)) throw new System.Exception(\"input press mismatch\");\n"
                << "            Vector2 p = Input.mousePosition; Vector2 d = Input.mouseDelta; Vector2 w = Input.mouseScrollDelta;\n"
                << "            if (p.x != 104 || p.y != 97 || d.x != 4 || d.y != -3 || w.y != 1.5f) throw new System.Exception(\"pointer mismatch\");\n"
                << "            transform.localPosition = new Vector3(d.x, d.y, w.y);\n"
                << "        } else {\n"
                << "            if (Input.GetKey(KeyCode.A) || Input.GetKeyDown(KeyCode.A) || !Input.GetKeyUp(KeyCode.A)) throw new System.Exception(\"input release mismatch\");\n"
                << "            gameObject.name = \"Input Passed\";\n"
                << "        }\n"
                << "        frame++;\n"
                << "    }\n"
                << "}\n"
                << "public class TimeProbe : Behaviour {\n"
                << "    private void Update() { transform.localPosition = new Vector3((float)Time.frameCount, Time.time, Time.deltaTime); }\n"
                << "}\n"
                << "public class LifetimeProbe : Behaviour {\n"
                << "    private bool ran;\n"
                << "    private void Update() {\n"
                << "        if (ran) return;\n"
                << "        GameObject spawned = new GameObject(\"Managed Spawn\");\n"
                << "        GameObject survivor = new GameObject(\"Managed Survivor\");\n"
                << "        GameObject? found = GameObject.Find(\"Managed Spawn\");\n"
                << "        if (found == null || found.GetInstanceID() != spawned.GetInstanceID()) throw new System.Exception(\"find mismatch\");\n"
                << "        GameObject.Destroy(spawned);\n"
                << "        GameObject.Destroy(spawned);\n"
                << "        GameObject.Destroy(gameObject);\n"
                << "        ran = true;\n"
                << "    }\n"
                << "}\n"
                << "public class CoroutineProbe : Behaviour {\n"
                << "    private System.Collections.IEnumerator Routine() {\n"
                << "        gameObject.name = \"Coroutine Started\";\n"
                << "        yield return null;\n"
                << "        transform.localPosition = transform.localPosition + new Vector3(1, 0, 0);\n"
                << "        yield return new WaitForSeconds(0.03f);\n"
                << "        gameObject.name = \"Coroutine Done\";\n"
                << "    }\n"
                << "    private void Start() { StartCoroutine(Routine()); }\n"
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
                    "NEngine.API.dll") &&
                std::filesystem::exists(
                    integration_build
                        .output_directory /
                    "NEngine.Bridge.dll"),
                "real dotnet SDK builds gameplay assembly plus dedicated API and stable Bridge dependencies into deterministic output");


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

                        check(
                            managed_runtime.reset_time(),
                            "managed Time clock resets explicitly before simulation");

                        ManagedScriptSystem
                            time_system;

                        time_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            time_world;

                        const auto time_entity_a =
                            time_world.create(
                                "Time A");

                        const auto time_entity_b =
                            time_world.create(
                                "Time B");

                        auto* time_script_a =
                            time_world.add_component<
                                ScriptBehaviour>(
                                    time_entity_a,
                                    script_behaviour_type());

                        if (time_script_a) {
                            time_script_a->type_name =
                                "TimeProbe";
                        }

                        auto* time_script_b =
                            time_world.add_component<
                                ScriptBehaviour>(
                                    time_entity_b,
                                    script_behaviour_type());

                        if (time_script_b) {
                            time_script_b->type_name =
                                "TimeProbe";
                        }

                        const bool time_scripts_ready =
                            time_script_a != nullptr &&
                            time_script_b != nullptr;

                        std::string
                            time_error;

                        const auto time_tick_1 =
                            time_system.update(
                                time_world,
                                0.25f,
                                &time_error);

                        const auto* time_transform_a =
                            time_world.transform(
                                time_entity_a);

                        const auto* time_transform_b =
                            time_world.transform(
                                time_entity_b);

                        check(
                            time_scripts_ready &&
                            time_tick_1.created == 2u &&
                            time_tick_1.started == 2u &&
                            time_tick_1.updated == 2u &&
                            time_tick_1.unresolved == 0u &&
                            time_transform_a &&
                            time_transform_b &&
                            time_transform_a->local_position.x == 1.0f &&
                            time_transform_b->local_position.x == 1.0f &&
                            time_transform_a->local_position.y == 0.25f &&
                            time_transform_b->local_position.y == 0.25f &&
                            time_transform_a->local_position.z == 0.25f &&
                            time_transform_b->local_position.z == 0.25f,
                            "managed Time advances once per simulation tick regardless of Behaviour count");

                        const auto time_tick_2 =
                            time_system.update(
                                time_world,
                                0.5f,
                                &time_error);

                        check(
                            time_tick_2.created == 0u &&
                            time_tick_2.updated == 2u &&
                            time_tick_2.unresolved == 0u &&
                            time_transform_a &&
                            time_transform_b &&
                            time_transform_a->local_position.x == 2.0f &&
                            time_transform_b->local_position.x == 2.0f &&
                            time_transform_a->local_position.y == 0.75f &&
                            time_transform_b->local_position.y == 0.75f &&
                            time_transform_a->local_position.z == 0.5f &&
                            time_transform_b->local_position.z == 0.5f,
                            "managed Time frame count and accumulated time stay global across multiple Behaviours");

                        time_system.clear();

                        check(
                            managed_runtime.reset_time(),
                            "managed Time clock resets after multi-Behaviour regression probe");

                        ManagedScriptSystem
                            lifetime_system;

                        lifetime_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            lifetime_world;

                        const auto lifetime_entity =
                            lifetime_world.create(
                                "Lifetime Root");

                        auto* lifetime_script =
                            lifetime_world.add_component<
                                ScriptBehaviour>(
                                    lifetime_entity,
                                    script_behaviour_type());

                        if (lifetime_script) {
                            lifetime_script->type_name =
                                "LifetimeProbe";
                        }

                        std::string
                            lifetime_error;

                        const auto lifetime_tick =
                            lifetime_system.update(
                                lifetime_world,
                                1.0f / 60.0f,
                                &lifetime_error);

                        nengine::core::Entity
                            spawned_entity =
                                nengine::core::Entity::invalid();

                        for (const auto candidate :
                             lifetime_world.entities()) {

                            if (lifetime_world.name(
                                    candidate) ==
                                "Managed Spawn") {

                                spawned_entity =
                                    candidate;
                                break;
                            }
                        }

                        check(
                            lifetime_script &&
                            lifetime_tick.created == 1u &&
                            lifetime_tick.started == 1u &&
                            lifetime_tick.updated == 1u &&
                            lifetime_tick.destroyed == 1u &&
                            lifetime_tick.unresolved == 0u &&
                            !lifetime_world.is_alive(
                                lifetime_entity) &&
                            !lifetime_world.is_alive(
                                spawned_entity) &&
                            lifetime_world.size() == 1u &&
                            lifetime_system.instance_count() == 0u &&
                            managed_runtime.instance_count() == 0,
                            "managed GameObject create/find deferred foreign destroy duplicate destroy and self-destroy round-trip through native World");

                        lifetime_system.clear();

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

                            const auto hierarchy_parent =
                                script_world.create(
                                    "Hierarchy Parent");

                            const auto hierarchy_child =
                                script_world.create(
                                    "Hierarchy Child");

                            check(
                                script_world.set_parent(
                                    hierarchy_child,
                                    hierarchy_parent),
                                "native hierarchy fixture parents managed script entity");

                            auto* hierarchy_parent_transform =
                                script_world.transform(
                                    hierarchy_parent);

                            if (hierarchy_parent_transform) {
                                hierarchy_parent_transform
                                    ->local_position = {
                                        3.0f,
                                        4.0f,
                                        5.0f
                                    };
                            }

                            auto* hierarchy_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        hierarchy_child,
                                        script_behaviour_type());

                            if (hierarchy_script) {
                                hierarchy_script->type_name =
                                    "HierarchyProbe";
                            }

                            const auto hierarchy_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            const auto* hierarchy_child_transform =
                                script_world.transform(
                                    hierarchy_child);

                            check(
                                hierarchy_script &&
                                hierarchy_tick.created == 1u &&
                                hierarchy_tick.started == 1u &&
                                hierarchy_tick.updated == 1u &&
                                hierarchy_tick.unresolved == 0u &&
                                hierarchy_parent_transform &&
                                hierarchy_parent_transform
                                    ->local_position.x == 5.0f &&
                                hierarchy_parent_transform
                                    ->local_position.y == 4.0f &&
                                hierarchy_parent_transform
                                    ->local_position.z == 5.0f &&
                                script_world.name(
                                    hierarchy_parent) ==
                                    "Managed Parent" &&
                                hierarchy_child_transform &&
                                !hierarchy_child_transform
                                    ->parent.valid() &&
                                script_world.children(
                                    hierarchy_parent)
                                    .empty(),
                                "managed ABI v6 callbacks mutate parent GameObject Transform and hierarchy immediately in native World");

                            script_world.destroy(
                                hierarchy_child);

                            const auto hierarchy_cleanup =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            check(
                                hierarchy_cleanup.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "hierarchy callback fixture releases managed instance before hot reload");

                            nengine::input::InputState
                                managed_input;

                            managed_input.set_pointer_position(
                                100.0f,
                                100.0f);

                            managed_input.begin_frame();

                            managed_input.set_pointer_position(
                                104.0f,
                                97.0f);

                            managed_input.add_wheel(
                                1.5f);

                            managed_input.set_key(
                                nengine::input::Key::A,
                                true);

                            managed_runtime.bind_input(
                                &managed_input);

                            const auto input_entity =
                                script_world.create(
                                    "Input Target");

                            auto* input_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        input_entity,
                                        script_behaviour_type());

                            if (input_script) {
                                input_script->type_name =
                                    "InputProbe";
                            }

                            auto* input_transform =
                                script_world.transform(
                                    input_entity);

                            const auto input_press_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                input_script &&
                                input_press_tick.created == 1u &&
                                input_press_tick.started == 1u &&
                                input_press_tick.updated == 1u &&
                                input_press_tick.unresolved == 0u &&
                                input_transform &&
                                input_transform
                                    ->local_position.x == 4.0f &&
                                input_transform
                                    ->local_position.y == -3.0f &&
                                input_transform
                                    ->local_position.z == 1.5f,
                                "managed Input API exposes key-down pointer delta and wheel from native InputState");

                            managed_input.begin_frame();

                            managed_input.set_key(
                                nengine::input::Key::A,
                                false);

                            const auto input_release_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                input_release_tick.updated == 1u &&
                                input_release_tick.unresolved == 0u &&
                                script_world.name(
                                    input_entity) ==
                                    "Input Passed",
                                "managed Input API exposes key-up transition on following frame");

                            script_world.destroy(
                                input_entity);

                            const auto input_cleanup =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            check(
                                input_cleanup.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "managed Input fixture releases instance before hot reload");

                            const auto coroutine_entity =
                                script_world.create(
                                    "Coroutine Target");

                            auto* coroutine_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        coroutine_entity,
                                        script_behaviour_type());

                            if (coroutine_script) {
                                coroutine_script->type_name =
                                    "CoroutineProbe";
                            }

                            auto* coroutine_transform =
                                script_world.transform(
                                    coroutine_entity);

                            const auto coroutine_tick_1 =
                                script_system.update(
                                    script_world,
                                    0.02f,
                                    &system_error);

                            check(
                                coroutine_script &&
                                coroutine_tick_1.created == 1u &&
                                coroutine_tick_1.started == 1u &&
                                coroutine_tick_1.updated == 1u &&
                                coroutine_tick_1.unresolved == 0u &&
                                coroutine_transform &&
                                coroutine_transform
                                    ->local_position.x == 0.0f &&
                                script_world.name(
                                    coroutine_entity) ==
                                    "Coroutine Started",
                                "managed coroutine Start primes through first yield and defers continuation for one frame");

                            const auto coroutine_tick_2 =
                                script_system.update(
                                    script_world,
                                    0.02f,
                                    &system_error);

                            check(
                                coroutine_tick_2.updated == 1u &&
                                coroutine_tick_2.unresolved == 0u &&
                                coroutine_transform &&
                                coroutine_transform
                                    ->local_position.x == 1.0f &&
                                script_world.name(
                                    coroutine_entity) ==
                                    "Coroutine Started",
                                "yield return null resumes on following managed update and enters WaitForSeconds");

                            const auto coroutine_tick_3 =
                                script_system.update(
                                    script_world,
                                    0.02f,
                                    &system_error);

                            check(
                                coroutine_tick_3.updated == 1u &&
                                coroutine_tick_3.unresolved == 0u &&
                                script_world.name(
                                    coroutine_entity) ==
                                    "Coroutine Started",
                                "WaitForSeconds remains suspended before requested duration elapses");

                            const auto coroutine_tick_4 =
                                script_system.update(
                                    script_world,
                                    0.02f,
                                    &system_error);

                            check(
                                coroutine_tick_4.updated == 1u &&
                                coroutine_tick_4.unresolved == 0u &&
                                script_world.name(
                                    coroutine_entity) ==
                                    "Coroutine Done",
                                "WaitForSeconds resumes coroutine after accumulated managed frame time");

                            script_world.destroy(
                                coroutine_entity);

                            const auto coroutine_cleanup =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            check(
                                coroutine_cleanup.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "destroying coroutine Behaviour stops scheduler state and releases managed instance");

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
                                    << "    private void Update() { transform.localPosition = transform.localPosition + new Vector3(10, 0, 0); gameObject.name = \"Reloaded\"; }\n"
                                    << "}\n";
                            }

                            ManagedBuildConfig
                                reload_build =
                                    integration_build;

                            reload_build.output_directory =
                                integration_root /
                                "Library" /
                                "ManagedBuild" /
                                "Reload";

                            const auto rebuilt =
                                run_managed_build(
                                    reload_build);

                            check(
                                rebuilt.success &&
                                std::filesystem::exists(
                                    rebuilt.plan
                                        .assembly_path),
                                "second gameplay build succeeds into independent hot-reload output");

                            if (rebuilt.success) {
                                check(
                                    managed_runtime.reload_gameplay(
                                        rebuilt.plan
                                            .assembly_path,
                                        "IntegrationScripts") &&
                                    managed_runtime
                                        .gameplay_loaded(),
                                    "ManagedRuntime reloads gameplay DLL through stable collectible Bridge without restarting hostfxr");

                                const auto reload_entity =
                                    script_world.create(
                                        "Reload Target");

                                auto* reload_script =
                                    script_world.add_component<
                                        ScriptBehaviour>(
                                            reload_entity,
                                            script_behaviour_type());

                                if (reload_script) {
                                    reload_script->type_name =
                                        "Example";
                                }

                                auto* reload_transform =
                                    script_world.transform(
                                        reload_entity);

                                if (reload_transform) {
                                    reload_transform
                                        ->local_position = {
                                            1.0f,
                                            2.0f,
                                            3.0f
                                        };
                                }

                                const auto reload_tick =
                                    script_system.update(
                                        script_world,
                                        1.0f / 60.0f,
                                        &system_error);

                                check(
                                    reload_script &&
                                    reload_tick.created == 1u &&
                                    reload_tick.started == 1u &&
                                    reload_tick.updated == 1u &&
                                    reload_tick.unresolved == 0u &&
                                    reload_transform &&
                                    reload_transform
                                        ->local_position.x ==
                                        11.0f &&
                                    reload_transform
                                        ->local_position.y ==
                                        2.0f &&
                                    reload_transform
                                        ->local_position.z ==
                                        3.0f &&
                                    script_world.name(
                                        reload_entity) ==
                                        "Reloaded",
                                    "hot-reloaded gameplay assembly executes new C# Behaviour code against existing native runtime");

                                script_world.destroy(
                                    reload_entity);

                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                                check(
                                    script_system
                                        .instance_count() ==
                                        0u &&
                                    managed_runtime
                                        .instance_count() ==
                                        0,
                                    "hot-reload test releases all managed instances before runtime shutdown");
                            }


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
