#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
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

struct ManagedCameraFixture {
    bool enabled{true};
    std::int64_t projection{0};
    float field_of_view{60.0f};
    float near_clip{0.1f};
    float far_clip{1000.0f};
    float orthographic_size{5.0f};
};

struct ManagedLightFixture {
    bool enabled{true};
    std::int64_t type{0};
    nengine::core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{10.0f};
    float spot_angle{30.0f};
    bool cast_shadows{true};
};

struct ManagedMeshFixture {
    std::string mesh{
        "11111111111111112222222222222222"};
    std::string material{
        "33333333333333334444444444444444"};
    bool enabled{true};
    bool cast_shadows{true};
    bool receive_shadows{true};
};

struct ManagedSpriteFixture {
    std::string texture{
        "55555555555555556666666666666666"};
    bool enabled{true};
    float pixels_per_unit{100.0f};
    std::int64_t sort_order{0};
    bool flip_x{false};
    bool flip_y{false};
};

struct ManagedSpriteAnimatorFixture {
    std::string clip{
        "77777777777777778888888888888888"};
    bool enabled{true};
    bool playing{true};
    bool loop{true};
    float speed{1.0f};
    float time_seconds{0.5f};
};

struct ManagedRigidbodyFixture {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    nengine::core::Vec3 linear_velocity{};
};

struct ManagedBoxColliderFixture {
    bool enabled{true};
    bool is_trigger{false};
    nengine::core::Vec3 center{};
    nengine::core::Vec3 size{1.0f, 1.0f, 1.0f};
};

struct ManagedRigidbody2DFixture {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    nengine::core::Vec3 linear_velocity{};
};

struct ManagedBoxCollider2DFixture {
    bool enabled{true};
    bool is_trigger{false};
    nengine::core::Vec3 center{};
    nengine::core::Vec3 size{1.0f, 1.0f, 0.0f};
};

bool read_managed_render_property(
    void*,
    const nengine::core::World& world,
    nengine::core::Entity entity,
    std::string_view component,
    std::string_view property,
    nengine::core::PropertyValue& output) {

    const auto type =
        nengine::core::ComponentRegistry::stable_id(
            component);

    if (component == "NEngine.Camera") {
        const auto* value =
            world.get_component<
                ManagedCameraFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Projection")
            output = value->projection;
        else if (property == "Vertical FOV")
            output = static_cast<double>(value->field_of_view);
        else if (property == "Near Clip")
            output = static_cast<double>(value->near_clip);
        else if (property == "Far Clip")
            output = static_cast<double>(value->far_clip);
        else if (property == "Orthographic Size")
            output = static_cast<double>(value->orthographic_size);
        else
            return false;

        return true;
    }

    if (component == "NEngine.Light") {
        const auto* value =
            world.get_component<
                ManagedLightFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Type")
            output = value->type;
        else if (property == "Color")
            output = value->color;
        else if (property == "Intensity")
            output = static_cast<double>(value->intensity);
        else if (property == "Range")
            output = static_cast<double>(value->range);
        else if (property == "Spot Angle")
            output = static_cast<double>(value->spot_angle);
        else if (property == "Cast Shadows")
            output = value->cast_shadows;
        else
            return false;

        return true;
    }

    if (component == "NEngine.MeshRenderer") {
        const auto* value =
            world.get_component<
                ManagedMeshFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Mesh")
            output = value->mesh;
        else if (property == "Material")
            output = value->material;
        else if (property == "Enabled")
            output = value->enabled;
        else if (property == "Cast Shadows")
            output = value->cast_shadows;
        else if (property == "Receive Shadows")
            output = value->receive_shadows;
        else
            return false;

        return true;
    }

    if (component == "NEngine.SpriteRenderer") {
        const auto* value =
            world.get_component<
                ManagedSpriteFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Texture")
            output = value->texture;
        else if (property == "Enabled")
            output = value->enabled;
        else if (property == "Pixels Per Unit")
            output = static_cast<double>(value->pixels_per_unit);
        else if (property == "Sort Order")
            output = value->sort_order;
        else if (property == "Flip X")
            output = value->flip_x;
        else if (property == "Flip Y")
            output = value->flip_y;
        else
            return false;

        return true;
    }

    if (component == "NEngine.SpriteAnimator") {
        const auto* value =
            world.get_component<
                ManagedSpriteAnimatorFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Clip")
            output = value->clip;
        else if (property == "Enabled")
            output = value->enabled;
        else if (property == "Playing")
            output = value->playing;
        else if (property == "Loop")
            output = value->loop;
        else if (property == "Speed")
            output = static_cast<double>(value->speed);
        else if (property == "Time")
            output = static_cast<double>(value->time_seconds);
        else
            return false;

        return true;
    }


    if (component == "NEngine.Rigidbody") {
        const auto* value =
            world.get_component<
                ManagedRigidbodyFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Use Gravity")
            output = value->use_gravity;
        else if (property == "Is Kinematic")
            output = value->is_kinematic;
        else if (property == "Mass")
            output = static_cast<double>(value->mass);
        else if (property == "Gravity Scale")
            output = static_cast<double>(value->gravity_scale);
        else if (property == "Linear Velocity")
            output = value->linear_velocity;
        else
            return false;

        return true;
    }

    if (component == "NEngine.BoxCollider") {
        const auto* value =
            world.get_component<
                ManagedBoxColliderFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Is Trigger")
            output = value->is_trigger;
        else if (property == "Center")
            output = value->center;
        else if (property == "Size")
            output = value->size;
        else
            return false;

        return true;
    }

    if (component == "NEngine.Rigidbody2D") {
        const auto* value =
            world.get_component<
                ManagedRigidbody2DFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Use Gravity")
            output = value->use_gravity;
        else if (property == "Is Kinematic")
            output = value->is_kinematic;
        else if (property == "Mass")
            output = static_cast<double>(value->mass);
        else if (property == "Gravity Scale")
            output = static_cast<double>(value->gravity_scale);
        else if (property == "Linear Velocity")
            output = value->linear_velocity;
        else
            return false;

        return true;
    }

    if (component == "NEngine.BoxCollider2D") {
        const auto* value =
            world.get_component<
                ManagedBoxCollider2DFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled")
            output = value->enabled;
        else if (property == "Is Trigger")
            output = value->is_trigger;
        else if (property == "Center")
            output = value->center;
        else if (property == "Size")
            output = value->size;
        else
            return false;

        return true;
    }

    return false;
}

bool write_managed_render_property(
    void*,
    nengine::core::World& world,
    nengine::core::Entity entity,
    std::string_view component,
    std::string_view property,
    const nengine::core::PropertyValue& input) {

    const auto type =
        nengine::core::ComponentRegistry::stable_id(
            component);

    if (component == "NEngine.Camera") {
        auto* value =
            world.get_component<
                ManagedCameraFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;
            value->enabled = *typed;
        } else if (property == "Projection") {
            const auto* typed =
                std::get_if<std::int64_t>(&input);
            if (!typed) return false;
            value->projection = *typed;
        } else {
            const auto* typed =
                std::get_if<double>(&input);
            if (!typed) return false;

            if (property == "Vertical FOV")
                value->field_of_view = static_cast<float>(*typed);
            else if (property == "Near Clip")
                value->near_clip = static_cast<float>(*typed);
            else if (property == "Far Clip")
                value->far_clip = static_cast<float>(*typed);
            else if (property == "Orthographic Size")
                value->orthographic_size = static_cast<float>(*typed);
            else
                return false;
        }

        return true;
    }

    if (component == "NEngine.Light") {
        auto* value =
            world.get_component<
                ManagedLightFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled" ||
            property == "Cast Shadows") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;
            if (property == "Enabled")
                value->enabled = *typed;
            else
                value->cast_shadows = *typed;
        } else if (property == "Type") {
            const auto* typed =
                std::get_if<std::int64_t>(&input);
            if (!typed) return false;
            value->type = *typed;
        } else if (property == "Color") {
            const auto* typed =
                std::get_if<nengine::core::Vec3>(&input);
            if (!typed) return false;
            value->color = *typed;
        } else {
            const auto* typed =
                std::get_if<double>(&input);
            if (!typed) return false;
            if (property == "Intensity")
                value->intensity = static_cast<float>(*typed);
            else if (property == "Range")
                value->range = static_cast<float>(*typed);
            else if (property == "Spot Angle")
                value->spot_angle = static_cast<float>(*typed);
            else
                return false;
        }

        return true;
    }

    if (component == "NEngine.MeshRenderer") {
        auto* value =
            world.get_component<
                ManagedMeshFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Mesh" ||
            property == "Material") {
            const auto* typed =
                std::get_if<std::string>(
                    &input);

            if (!typed) return false;

            if (property == "Mesh")
                value->mesh = *typed;
            else
                value->material = *typed;
        } else {
            const auto* typed =
                std::get_if<bool>(
                    &input);

            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else if (property == "Cast Shadows")
                value->cast_shadows = *typed;
            else if (property == "Receive Shadows")
                value->receive_shadows = *typed;
            else
                return false;
        }

        return true;
    }

    if (component == "NEngine.SpriteRenderer") {
        auto* value =
            world.get_component<
                ManagedSpriteFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Texture") {
            const auto* typed =
                std::get_if<std::string>(
                    &input);
            if (!typed) return false;
            value->texture = *typed;
        } else if (property == "Enabled" ||
            property == "Flip X" ||
            property == "Flip Y") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else if (property == "Flip X")
                value->flip_x = *typed;
            else
                value->flip_y = *typed;
        } else if (property == "Pixels Per Unit") {
            const auto* typed =
                std::get_if<double>(&input);
            if (!typed) return false;
            value->pixels_per_unit =
                static_cast<float>(*typed);
        } else if (property == "Sort Order") {
            const auto* typed =
                std::get_if<std::int64_t>(&input);
            if (!typed) return false;
            value->sort_order = *typed;
        } else {
            return false;
        }

        return true;
    }

    if (component == "NEngine.SpriteAnimator") {
        auto* value =
            world.get_component<
                ManagedSpriteAnimatorFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Clip") {
            const auto* typed =
                std::get_if<std::string>(
                    &input);
            if (!typed) return false;
            value->clip = *typed;
            value->time_seconds = 0.0f;
        } else if (property == "Enabled" ||
                   property == "Playing" ||
                   property == "Loop") {
            const auto* typed =
                std::get_if<bool>(
                    &input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else if (property == "Playing")
                value->playing = *typed;
            else
                value->loop = *typed;
        } else if (property == "Speed" ||
                   property == "Time") {
            const auto* typed =
                std::get_if<double>(
                    &input);
            if (!typed || *typed < 0.0)
                return false;

            if (property == "Speed")
                value->speed =
                    static_cast<float>(*typed);
            else
                value->time_seconds =
                    static_cast<float>(*typed);
        } else {
            return false;
        }

        return true;
    }


    if (component == "NEngine.Rigidbody") {
        auto* value =
            world.get_component<
                ManagedRigidbodyFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled" ||
            property == "Use Gravity" ||
            property == "Is Kinematic") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else if (property == "Use Gravity")
                value->use_gravity = *typed;
            else
                value->is_kinematic = *typed;
        } else if (property == "Mass" ||
                   property == "Gravity Scale") {
            const auto* typed =
                std::get_if<double>(&input);
            if (!typed) return false;

            if (property == "Mass")
                value->mass = static_cast<float>(*typed);
            else
                value->gravity_scale = static_cast<float>(*typed);
        } else if (property == "Linear Velocity") {
            const auto* typed =
                std::get_if<nengine::core::Vec3>(&input);
            if (!typed) return false;
            value->linear_velocity = *typed;
        } else {
            return false;
        }

        return true;
    }

    if (component == "NEngine.BoxCollider") {
        auto* value =
            world.get_component<
                ManagedBoxColliderFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled" ||
            property == "Is Trigger") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else
                value->is_trigger = *typed;
        } else if (property == "Center" ||
                   property == "Size") {
            const auto* typed =
                std::get_if<nengine::core::Vec3>(&input);
            if (!typed) return false;

            if (property == "Center")
                value->center = *typed;
            else
                value->size = *typed;
        } else {
            return false;
        }

        return true;
    }

    if (component == "NEngine.Rigidbody2D") {
        auto* value =
            world.get_component<
                ManagedRigidbody2DFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled" ||
            property == "Use Gravity" ||
            property == "Is Kinematic") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else if (property == "Use Gravity")
                value->use_gravity = *typed;
            else
                value->is_kinematic = *typed;
        } else if (property == "Mass" ||
                   property == "Gravity Scale") {
            const auto* typed =
                std::get_if<double>(&input);
            if (!typed) return false;

            if (property == "Mass")
                value->mass = static_cast<float>(*typed);
            else
                value->gravity_scale = static_cast<float>(*typed);
        } else if (property == "Linear Velocity") {
            const auto* typed =
                std::get_if<nengine::core::Vec3>(&input);
            if (!typed) return false;
            value->linear_velocity = *typed;
        } else {
            return false;
        }

        return true;
    }

    if (component == "NEngine.BoxCollider2D") {
        auto* value =
            world.get_component<
                ManagedBoxCollider2DFixture>(
                    entity,
                    type);

        if (!value) return false;

        if (property == "Enabled" ||
            property == "Is Trigger") {
            const auto* typed =
                std::get_if<bool>(&input);
            if (!typed) return false;

            if (property == "Enabled")
                value->enabled = *typed;
            else
                value->is_trigger = *typed;
        } else if (property == "Center" ||
                   property == "Size") {
            const auto* typed =
                std::get_if<nengine::core::Vec3>(&input);
            if (!typed) return false;

            if (property == "Center")
                value->center = *typed;
            else
                value->size = *typed;
        } else {
            return false;
        }

        return true;
    }

    return false;
}

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
        api.find("activeInHierarchy") !=
            std::string::npos &&
        api.find("IsChildOf") !=
            std::string::npos &&
        api.find("DetachChildren") !=
            std::string::npos &&
        api.find("fieldOfView") !=
            std::string::npos &&
        api.find("LightType") !=
            std::string::npos &&
        api.find("pixelsPerUnit") !=
            std::string::npos &&
        api.find("receiveShadows") !=
            std::string::npos &&
        api.find("SpriteAnimator") !=
            std::string::npos &&
        api.find("Restart") !=
            std::string::npos &&
        api.find("Rigidbody2D") !=
            std::string::npos &&
        api.find("AddForce") !=
            std::string::npos &&
        api.find("BoxCollider2D") !=
            std::string::npos &&
        api.find("sealed class Collision") !=
            std::string::npos &&
        api.find("sealed class Collision2D") !=
            std::string::npos &&
        api.find("abstract class Collider") !=
            std::string::npos &&
        api.find("readonly struct AssetGuid") !=
            std::string::npos &&
        api.find("TryParse") !=
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
            "AbiVersion = 12") !=
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
        api.find(
            "NativePropertyValue") !=
                std::string::npos &&
        api.find(
            "TryGetProperty") !=
                std::string::npos &&
        api.find(
            "TryGetTextProperty") !=
                std::string::npos &&
        api.find(
            "SetTextProperty") !=
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
                std::string::npos &&
        bridge.find(
            "InvokeAwake") !=
                std::string::npos &&
        bridge.find(
            "InvokeEnable") !=
                std::string::npos &&
        bridge.find(
            "InvokeDisable") !=
                std::string::npos &&
        bridge.find(
            "InvokeFixedUpdate") !=
                std::string::npos &&
        bridge.find(
            "InvokeLateUpdate") !=
                std::string::npos &&
        bridge.find(
            "SetBehaviourEnabled") !=
                std::string::npos &&
        bridge.find(
            "GetBehaviourEnabled") !=
                std::string::npos,
        "managed bridge exposes activation FixedUpdate LateUpdate native World property input and frame-clock ABI v12 entries");

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
                << "    public int awakes;\n"
                << "    public int starts;\n"
                << "    public int updates;\n"
                << "    private void Awake() { awakes++; if (gameObject.name != \"Runtime Object\" && gameObject.name != \"Managed Example\") throw new System.Exception(\"Awake native state mismatch\"); gameObject.name = \"Managed Awakened\"; }\n"
                << "    private void Start() { if (awakes != 1 || gameObject.name != \"Managed Awakened\") throw new System.Exception(\"Awake/Start order mismatch\"); starts++; gameObject.name = \"Managed Renamed\"; }\n"
                << "    private void FixedUpdate() { if (System.MathF.Abs(Time.fixedDeltaTime - 0.02f) > 0.0001f || System.MathF.Abs(Time.deltaTime - 0.02f) > 0.0001f) throw new System.Exception(\"fixed delta mismatch\"); }\n"
                << "    private void Update() { updates++; var t = GetComponent<Transform>(); if (t == null) throw new System.Exception(\"Transform missing\"); t.localPosition = t.localPosition + new Vector3(1, 2, 3); }\n"
                << "}\n"
                << "public class DeactivateOnce : Behaviour {\n"
                << "    private void Update() { gameObject.SetActive(false); }\n"
                << "}\n"
                << "public class LifecycleProbe : Behaviour {\n"
                << "    private void Awake() { transform.localPosition = transform.localPosition + new Vector3(1, 0, 0); }\n"
                << "    private void OnEnable() { transform.localPosition = transform.localPosition + new Vector3(10, 0, 0); }\n"
                << "    private void Start() { transform.localPosition = transform.localPosition + new Vector3(100, 0, 0); }\n"
                << "    private void Update() { transform.localPosition = transform.localPosition + new Vector3(1000, 0, 0); }\n"
                << "    private void OnDisable() { transform.localPosition = transform.localPosition + new Vector3(10000, 0, 0); }\n"
                << "    private void OnDestroy() { _ = new GameObject(\"Lifecycle Destroyed\"); }\n"
                << "}\n"
                << "public class DisableSelf : Behaviour {\n"
                << "    private void Update() { enabled = false; }\n"
                << "    private void OnDisable() { gameObject.name = \"Self Disabled\"; }\n"
                << "}\n"
                << "public class HierarchyProbe : Behaviour {\n"
                << "    private void Update() {\n"
                << "        Transform? p = transform.parent;\n"
                << "        if (p == null) throw new System.Exception(\"parent missing\");\n"
                << "        if (!p.gameObject.HasComponent<Transform>()) throw new System.Exception(\"native transform missing\");\n"
                << "        if (!gameObject.activeInHierarchy) throw new System.Exception(\"activeInHierarchy mismatch\");\n"
                << "        if (transform.root.gameObject.GetInstanceID() != p.gameObject.GetInstanceID()) throw new System.Exception(\"root mismatch\");\n"
                << "        if (!transform.IsChildOf(p) || p.IsChildOf(transform)) throw new System.Exception(\"IsChildOf mismatch\");\n"
                << "        if (p.childCount != 1) throw new System.Exception(\"child count mismatch\");\n"
                << "        Transform c = p.GetChild(0);\n"
                << "        if (c.gameObject.GetInstanceID() != gameObject.GetInstanceID()) throw new System.Exception(\"child identity mismatch\");\n"
                << "        Transform? found = p.Find(gameObject.name);\n"
                << "        if (found == null || found.gameObject.GetInstanceID() != gameObject.GetInstanceID()) throw new System.Exception(\"Transform.Find mismatch\");\n"
                << "        p.gameObject.name = \"Managed Parent\";\n"
                << "        p.localPosition = p.localPosition + new Vector3(2, 0, 0);\n"
                << "        p.DetachChildren();\n"
                << "        if (transform.parent != null) throw new System.Exception(\"DetachChildren mismatch\");\n"
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
                << "public class RenderPropertyProbe : Behaviour {\n"
                << "    private void Update() {\n"
                << "        Camera? camera = GetComponent<Camera>();\n"
                << "        Light? light = GetComponent<Light>();\n"
                << "        MeshRenderer? mesh = GetComponent<MeshRenderer>();\n"
                << "        SpriteRenderer? sprite = GetComponent<SpriteRenderer>();\n"
                << "        SpriteAnimator? animator = GetComponent<SpriteAnimator>();\n"
                << "        Rigidbody? body = GetComponent<Rigidbody>(); BoxCollider? box = GetComponent<BoxCollider>(); Rigidbody2D? body2d = GetComponent<Rigidbody2D>(); BoxCollider2D? box2d = GetComponent<BoxCollider2D>();\n"
                << "        if (camera == null || light == null || mesh == null || sprite == null || animator == null || body == null || box == null || body2d == null || box2d == null) throw new System.Exception(\"component proxy missing\");\n"
                << "        if (!camera.enabled || camera.orthographic || System.MathF.Abs(camera.fieldOfView - 60f) > 0.001f) throw new System.Exception(\"camera read mismatch\");\n"
                << "        camera.enabled = false; camera.orthographic = true; camera.fieldOfView = 72f; camera.nearClipPlane = 0.25f; camera.farClipPlane = 750f; camera.orthographicSize = 8f;\n"
                << "        if (light.type != LightType.Directional || System.MathF.Abs(light.intensity - 1f) > 0.001f) throw new System.Exception(\"light read mismatch\");\n"
                << "        light.enabled = false; light.type = LightType.Point; light.color = new Color(0.2f, 0.3f, 0.4f); light.intensity = 2.5f; light.range = 20f; light.spotAngle = 45f; light.shadows = false;\n"
                << "        if (mesh.mesh.ToString() != \"11111111111111112222222222222222\" || mesh.material.ToString() != \"33333333333333334444444444444444\" || sprite.texture.ToString() != \"55555555555555556666666666666666\" || animator.clip.ToString() != \"77777777777777778888888888888888\") throw new System.Exception(\"asset guid read mismatch\");\n"
                << "        if (!animator.enabled || !animator.playing || !animator.loop || System.MathF.Abs(animator.speed - 1f) > 0.001f || System.MathF.Abs(animator.time - 0.5f) > 0.001f) throw new System.Exception(\"animator initial state mismatch\");\n"
                << "        animator.Pause(); if (animator.playing) throw new System.Exception(\"animator pause mismatch\"); animator.time = 0.75f; animator.Restart(); if (!animator.playing || System.MathF.Abs(animator.time) > 0.001f) throw new System.Exception(\"animator restart mismatch\");\n"
                << "        if (!AssetGuid.TryParse(\"aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbb\", out AssetGuid nextMesh) || !AssetGuid.TryParse(\"ccccccccccccccccdddddddddddddddd\", out AssetGuid nextMaterial) || !AssetGuid.TryParse(\"eeeeeeeeeeeeeeeeffffffffffffffff\", out AssetGuid nextTexture) || !AssetGuid.TryParse(\"9999999999999999aaaaaaaaaaaaaaaa\", out AssetGuid nextClip)) throw new System.Exception(\"asset guid parse mismatch\");\n"
                << "        animator.Play(nextClip); if (!animator.playing || animator.clip != nextClip || System.MathF.Abs(animator.time) > 0.001f) throw new System.Exception(\"animator Play clip mismatch\"); animator.speed = 1.5f; animator.loop = false; animator.enabled = false; animator.Stop();\n"
                << "        mesh.mesh = nextMesh; mesh.material = nextMaterial; mesh.enabled = false; mesh.castShadows = false; mesh.receiveShadows = false;\n"
                << "        sprite.texture = nextTexture; sprite.enabled = false; sprite.pixelsPerUnit = 64f; sprite.sortingOrder = 7; sprite.flipX = true; sprite.flipY = true;\n"
                << "        if (System.MathF.Abs(body.mass - 1f) > 0.001f || !body.useGravity || body.isKinematic) throw new System.Exception(\"rigidbody read mismatch\"); body.useGravity = false; body.mass = 2.5f; body.gravityScale = 0.5f; body.velocity = new Vector3(1, 2, 3); body.AddForce(new Vector3(2.5f, 0, 0)); body.isKinematic = true; body.enabled = false;\n"
                << "        box.isTrigger = true; box.center = new Vector3(0.1f, 0.2f, 0.3f); box.size = new Vector3(2, 3, 4);\n"
                << "        body2d.useGravity = false; body2d.mass = 3f; body2d.gravityScale = 0.25f; body2d.velocity = new Vector2(4, 5); body2d.AddForce(new Vector2(3, 0)); body2d.isKinematic = true;\n"
                << "        box2d.isTrigger = true; box2d.center = new Vector2(0.5f, 0.75f); box2d.size = new Vector2(6, 7);\n"
                << "        gameObject.name = \"Render Physics Properties Passed\";\n"
                << "    }\n"
                << "}\n"
                << "public class PhysicsEventProbe : Behaviour {\n"
                << "    private int mask;\n"
                << "    private void Mark(int bit, string otherName) { if (otherName != \"Physics Other\") throw new System.Exception(\"physics other mismatch\"); mask |= bit; if (mask == 4095) gameObject.name = \"Physics Events Passed\"; }\n"
                << "    private void OnCollisionEnter(Collision c) { if (System.MathF.Abs(c.normal.x - 1f) > 0.001f || System.MathF.Abs(c.penetration - 0.25f) > 0.001f) throw new System.Exception(\"collision payload mismatch\"); Mark(1, c.gameObject.name); }\n"
                << "    private void OnCollisionStay(Collision c) { Mark(2, c.gameObject.name); }\n"
                << "    private void OnCollisionExit(Collision c) { Mark(4, c.gameObject.name); }\n"
                << "    private void OnTriggerEnter(Collider c) { if (!c.isTrigger) throw new System.Exception(\"trigger collider mismatch\"); Mark(8, c.gameObject.name); }\n"
                << "    private void OnTriggerStay(Collider c) { Mark(16, c.gameObject.name); }\n"
                << "    private void OnTriggerExit(Collider c) { Mark(32, c.gameObject.name); }\n"
                << "    private void OnCollisionEnter2D(Collision2D c) { if (System.MathF.Abs(c.normal.y - 1f) > 0.001f) throw new System.Exception(\"collision2d payload mismatch\"); Mark(64, c.gameObject.name); }\n"
                << "    private void OnCollisionStay2D(Collision2D c) { Mark(128, c.gameObject.name); }\n"
                << "    private void OnCollisionExit2D(Collision2D c) { Mark(256, c.gameObject.name); }\n"
                << "    private void OnTriggerEnter2D(Collider2D c) { if (!c.isTrigger) throw new System.Exception(\"trigger2d collider mismatch\"); Mark(512, c.gameObject.name); }\n"
                << "    private void OnTriggerStay2D(Collider2D c) { Mark(1024, c.gameObject.name); }\n"
                << "    private void OnTriggerExit2D(Collider2D c) { Mark(2048, c.gameObject.name); }\n"
                << "}\n"
                << "public class FixedSystemProbe : Behaviour {\n"
                << "    private void FixedUpdate() {\n"
                << "        if (System.MathF.Abs(Time.fixedDeltaTime - 0.02f) > 0.0001f || System.MathF.Abs(Time.deltaTime - 0.02f) > 0.0001f) throw new System.Exception(\"fixed system delta mismatch\");\n"
                << "        transform.localPosition = transform.localPosition + new Vector3(10, 0, 0);\n"
                << "    }\n"
                << "    private void Update() { transform.localPosition = transform.localPosition + new Vector3(1, 0, 0); }\n"
                << "    private void LateUpdate() { transform.localPosition = transform.localPosition + new Vector3(0, 1, 0); }\n"
                << "}\n"
                << "public class LateOrderProbe : Behaviour {\n"
                << "    private static ulong frame;\n"
                << "    private static int updates;\n"
                << "    private void Update() {\n"
                << "        if (frame != Time.frameCount) { frame = Time.frameCount; updates = 0; }\n"
                << "        updates++;\n"
                << "        transform.localPosition = transform.localPosition + new Vector3(1, 0, 0);\n"
                << "    }\n"
                << "    private void LateUpdate() {\n"
                << "        if (updates != 2) throw new System.Exception(\"LateUpdate ran before all Updates\");\n"
                << "        transform.localPosition = transform.localPosition + new Vector3(0, 1, 0);\n"
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
                        managed_runtime.bind_property_access(
                            nullptr,
                            &read_managed_render_property,
                            &write_managed_render_property);

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
                                managed_runtime.awake(
                                    behaviour) &&
                                managed_runtime.start(
                                    behaviour),
                                "managed lifecycle invokes explicit Awake before Start");

                            check(
                                managed_runtime.fixed_update(
                                    behaviour,
                                    0.02f),
                                "managed lifecycle invokes FixedUpdate with fixed delta");

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
                            property_system;

                        property_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            property_world;

                        const auto property_entity =
                            property_world.create(
                                "Render Property Probe");

                        const auto camera_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.Camera");

                        const auto light_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.Light");

                        const auto mesh_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.MeshRenderer");

                        const auto sprite_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.SpriteRenderer");

                        const auto animator_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.SpriteAnimator");

                        const auto rigidbody_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.Rigidbody");

                        const auto box_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.BoxCollider");

                        const auto rigidbody2d_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.Rigidbody2D");

                        const auto box2d_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.BoxCollider2D");

                        property_world.add_component<
                            ManagedCameraFixture>(
                                property_entity,
                                camera_type);

                        property_world.add_component<
                            ManagedLightFixture>(
                                property_entity,
                                light_type);

                        property_world.add_component<
                            ManagedMeshFixture>(
                                property_entity,
                                mesh_type);

                        property_world.add_component<
                            ManagedSpriteFixture>(
                                property_entity,
                                sprite_type);

                        property_world.add_component<
                            ManagedSpriteAnimatorFixture>(
                                property_entity,
                                animator_type);

                        property_world.add_component<
                            ManagedRigidbodyFixture>(
                                property_entity,
                                rigidbody_type);

                        property_world.add_component<
                            ManagedBoxColliderFixture>(
                                property_entity,
                                box_type);

                        property_world.add_component<
                            ManagedRigidbody2DFixture>(
                                property_entity,
                                rigidbody2d_type);

                        property_world.add_component<
                            ManagedBoxCollider2DFixture>(
                                property_entity,
                                box2d_type);

                        auto* property_script =
                            property_world.add_component<
                                ScriptBehaviour>(
                                    property_entity,
                                    script_behaviour_type());

                        if (property_script) {
                            property_script->type_name =
                                "RenderPropertyProbe";
                        }

                        std::string property_error;

                        const auto property_tick =
                            property_system.update(
                                property_world,
                                1.0f / 60.0f,
                                &property_error);

                        const auto* camera_fixture =
                            property_world.get_component<
                                ManagedCameraFixture>(
                                    property_entity,
                                    camera_type);

                        const auto* light_fixture =
                            property_world.get_component<
                                ManagedLightFixture>(
                                    property_entity,
                                    light_type);

                        const auto* mesh_fixture =
                            property_world.get_component<
                                ManagedMeshFixture>(
                                    property_entity,
                                    mesh_type);

                        const auto* sprite_fixture =
                            property_world.get_component<
                                ManagedSpriteFixture>(
                                    property_entity,
                                    sprite_type);

                        const auto* animator_fixture =
                            property_world.get_component<
                                ManagedSpriteAnimatorFixture>(
                                    property_entity,
                                    animator_type);

                        const auto* rigidbody_fixture =
                            property_world.get_component<
                                ManagedRigidbodyFixture>(
                                    property_entity,
                                    rigidbody_type);

                        const auto* box_fixture =
                            property_world.get_component<
                                ManagedBoxColliderFixture>(
                                    property_entity,
                                    box_type);

                        const auto* rigidbody2d_fixture =
                            property_world.get_component<
                                ManagedRigidbody2DFixture>(
                                    property_entity,
                                    rigidbody2d_type);

                        const auto* box2d_fixture =
                            property_world.get_component<
                                ManagedBoxCollider2DFixture>(
                                    property_entity,
                                    box2d_type);

                        check(
                            property_script &&
                            property_tick.created == 1u &&
                            property_tick.started == 1u &&
                            property_tick.updated == 1u &&
                            property_tick.unresolved == 0u &&
                            property_world.name(
                                property_entity) ==
                                "Render Physics Properties Passed" &&
                            camera_fixture &&
                            !camera_fixture->enabled &&
                            camera_fixture->projection == 1 &&
                            std::abs(camera_fixture->field_of_view - 72.0f) < 0.001f &&
                            std::abs(camera_fixture->near_clip - 0.25f) < 0.001f &&
                            std::abs(camera_fixture->far_clip - 750.0f) < 0.001f &&
                            std::abs(camera_fixture->orthographic_size - 8.0f) < 0.001f &&
                            light_fixture &&
                            !light_fixture->enabled &&
                            light_fixture->type == 1 &&
                            std::abs(light_fixture->color.x - 0.2f) < 0.001f &&
                            std::abs(light_fixture->color.y - 0.3f) < 0.001f &&
                            std::abs(light_fixture->color.z - 0.4f) < 0.001f &&
                            std::abs(light_fixture->intensity - 2.5f) < 0.001f &&
                            std::abs(light_fixture->range - 20.0f) < 0.001f &&
                            std::abs(light_fixture->spot_angle - 45.0f) < 0.001f &&
                            !light_fixture->cast_shadows &&
                            mesh_fixture &&
                            mesh_fixture->mesh ==
                                "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbb" &&
                            mesh_fixture->material ==
                                "ccccccccccccccccdddddddddddddddd" &&
                            !mesh_fixture->enabled &&
                            !mesh_fixture->cast_shadows &&
                            !mesh_fixture->receive_shadows &&
                            sprite_fixture &&
                            sprite_fixture->texture ==
                                "eeeeeeeeeeeeeeeeffffffffffffffff" &&
                            !sprite_fixture->enabled &&
                            std::abs(sprite_fixture->pixels_per_unit - 64.0f) < 0.001f &&
                            sprite_fixture->sort_order == 7 &&
                            sprite_fixture->flip_x &&
                            sprite_fixture->flip_y &&
                            animator_fixture &&
                            animator_fixture->clip ==
                                "9999999999999999aaaaaaaaaaaaaaaa" &&
                            !animator_fixture->enabled &&
                            !animator_fixture->playing &&
                            !animator_fixture->loop &&
                            std::abs(animator_fixture->speed - 1.5f) < 0.001f &&
                            std::abs(animator_fixture->time_seconds) < 0.001f &&
                            rigidbody_fixture &&
                            !rigidbody_fixture->enabled &&
                            !rigidbody_fixture->use_gravity &&
                            rigidbody_fixture->is_kinematic &&
                            std::abs(rigidbody_fixture->mass - 2.5f) < 0.001f &&
                            std::abs(rigidbody_fixture->gravity_scale - 0.5f) < 0.001f &&
                            rigidbody_fixture->linear_velocity ==
                                nengine::core::Vec3{2.0f, 2.0f, 3.0f} &&
                            box_fixture &&
                            box_fixture->is_trigger &&
                            box_fixture->center ==
                                nengine::core::Vec3{0.1f, 0.2f, 0.3f} &&
                            box_fixture->size ==
                                nengine::core::Vec3{2.0f, 3.0f, 4.0f} &&
                            rigidbody2d_fixture &&
                            !rigidbody2d_fixture->use_gravity &&
                            rigidbody2d_fixture->is_kinematic &&
                            std::abs(rigidbody2d_fixture->mass - 3.0f) < 0.001f &&
                            std::abs(rigidbody2d_fixture->gravity_scale - 0.25f) < 0.001f &&
                            rigidbody2d_fixture->linear_velocity ==
                                nengine::core::Vec3{5.0f, 5.0f, 0.0f} &&
                            box2d_fixture &&
                            box2d_fixture->is_trigger &&
                            box2d_fixture->center ==
                                nengine::core::Vec3{0.5f, 0.75f, 0.0f} &&
                            box2d_fixture->size ==
                                nengine::core::Vec3{6.0f, 7.0f, 0.0f},
                            "managed render and physics component proxies round-trip through generic native property ABI");

                        property_system.clear(
                            &property_world);

                        ManagedScriptSystem
                            physics_event_system;

                        physics_event_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            physics_event_world;

                        const auto physics_event_target =
                            physics_event_world.create(
                                "Physics Event Target");

                        const auto physics_event_other =
                            physics_event_world.create(
                                "Physics Other");

                        const auto event_box_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.BoxCollider");

                        const auto event_box2d_type =
                            nengine::core::ComponentRegistry::stable_id(
                                "NEngine.BoxCollider2D");

                        auto* event_script =
                            physics_event_world.add_component<
                                ScriptBehaviour>(
                                    physics_event_target,
                                    script_behaviour_type());

                        if (event_script) {
                            event_script->type_name =
                                "PhysicsEventProbe";
                        }

                        auto* event_box =
                            physics_event_world.add_component<
                                ManagedBoxColliderFixture>(
                                    physics_event_other,
                                    event_box_type);

                        auto* event_box2d =
                            physics_event_world.add_component<
                                ManagedBoxCollider2DFixture>(
                                    physics_event_other,
                                    event_box2d_type);

                        if (event_box) {
                            event_box->is_trigger =
                                true;
                        }

                        if (event_box2d) {
                            event_box2d->is_trigger =
                                true;
                        }

                        std::string physics_event_error;

                        const auto event_start =
                            physics_event_system.fixed_update(
                                physics_event_world,
                                0.02f,
                                &physics_event_error);

                        bool all_physics_events = true;

                        for (int is2d = 0;
                             is2d <= 1;
                             ++is2d) {
                            for (int trigger = 0;
                                 trigger <= 1;
                                 ++trigger) {
                                for (int phase = 0;
                                     phase <= 2;
                                     ++phase) {

                                    const auto normal =
                                        is2d != 0
                                            ? nengine::core::Vec3{
                                                0.0f,
                                                1.0f,
                                                0.0f}
                                            : nengine::core::Vec3{
                                                1.0f,
                                                0.0f,
                                                0.0f};

                                    all_physics_events =
                                        physics_event_system
                                            .dispatch_physics_event(
                                                physics_event_world,
                                                physics_event_target,
                                                physics_event_other,
                                                phase,
                                                trigger != 0,
                                                is2d != 0,
                                                normal,
                                                phase == 2
                                                    ? 0.0f
                                                    : 0.25f,
                                                &physics_event_error) &&
                                        all_physics_events;
                                }
                            }
                        }

                        check(
                            event_script &&
                            event_start.created == 1u &&
                            event_start.started == 1u &&
                            event_start.unresolved == 0u &&
                            all_physics_events &&
                            physics_event_error.empty() &&
                            physics_event_world.name(
                                physics_event_target) ==
                                "Physics Events Passed",
                            "managed Collision Trigger 3D and 2D Enter Stay Exit callbacks receive native event payloads");

                        physics_event_system.clear(
                            &physics_event_world);

                        ManagedScriptSystem
                            fixed_system;

                        fixed_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            fixed_world;

                        const auto fixed_entity =
                            fixed_world.create(
                                "Fixed System");

                        auto* fixed_script =
                            fixed_world.add_component<
                                ScriptBehaviour>(
                                    fixed_entity,
                                    script_behaviour_type());

                        if (fixed_script) {
                            fixed_script->type_name =
                                "FixedSystemProbe";
                        }

                        std::string fixed_error;

                        const auto fixed_tick_1 =
                            fixed_system.fixed_update(
                                fixed_world,
                                0.02f,
                                &fixed_error);

                        const auto fixed_tick_2 =
                            fixed_system.fixed_update(
                                fixed_world,
                                0.02f,
                                &fixed_error);

                        const auto fixed_frame =
                            fixed_system.update(
                                fixed_world,
                                1.0f / 30.0f,
                                &fixed_error);

                        const auto* fixed_transform =
                            fixed_world.transform(
                                fixed_entity);

                        check(
                            fixed_script &&
                            fixed_tick_1.created == 1u &&
                            fixed_tick_1.awoken == 1u &&
                            fixed_tick_1.enabled == 1u &&
                            fixed_tick_1.started == 1u &&
                            fixed_tick_1.fixed_updated == 1u &&
                            fixed_tick_1.unresolved == 0u &&
                            fixed_tick_2.created == 0u &&
                            fixed_tick_2.started == 0u &&
                            fixed_tick_2.fixed_updated == 1u &&
                            fixed_tick_2.unresolved == 0u &&
                            fixed_frame.fixed_updated == 0u &&
                            fixed_frame.updated == 1u &&
                            fixed_frame.late_updated == 1u &&
                            fixed_frame.unresolved == 0u &&
                            fixed_transform &&
                            fixed_transform->local_position.x == 21.0f &&
                            fixed_transform->local_position.y == 1.0f,
                            "managed fixed-step callbacks run separately before one host Update and LateUpdate frame");

                        fixed_system.clear(
                            &fixed_world);

                        ManagedScriptSystem
                            late_system;

                        late_system.bind(
                            &managed_runtime);

                        nengine::core::World
                            late_world;

                        const auto late_entity_a =
                            late_world.create(
                                "Late A");

                        const auto late_entity_b =
                            late_world.create(
                                "Late B");

                        auto* late_script_a =
                            late_world.add_component<
                                ScriptBehaviour>(
                                    late_entity_a,
                                    script_behaviour_type());

                        if (late_script_a) {
                            late_script_a->type_name =
                                "LateOrderProbe";
                        }

                        auto* late_script_b =
                            late_world.add_component<
                                ScriptBehaviour>(
                                    late_entity_b,
                                    script_behaviour_type());

                        if (late_script_b) {
                            late_script_b->type_name =
                                "LateOrderProbe";
                        }

                        late_script_a =
                            late_world.get_component<
                                ScriptBehaviour>(
                                    late_entity_a,
                                    script_behaviour_type());

                        std::string late_error;

                        const auto late_tick =
                            late_system.update(
                                late_world,
                                1.0f / 60.0f,
                                &late_error);

                        const auto* late_transform_a =
                            late_world.transform(
                                late_entity_a);

                        const auto* late_transform_b =
                            late_world.transform(
                                late_entity_b);

                        check(
                            late_script_a &&
                            late_script_b &&
                            late_tick.created == 2u &&
                            late_tick.awoken == 2u &&
                            late_tick.enabled == 2u &&
                            late_tick.started == 2u &&
                            late_tick.updated == 2u &&
                            late_tick.late_updated == 2u &&
                            late_tick.unresolved == 0u &&
                            late_transform_a &&
                            late_transform_b &&
                            late_transform_a->local_position.x == 1.0f &&
                            late_transform_a->local_position.y == 1.0f &&
                            late_transform_b->local_position.x == 1.0f &&
                            late_transform_b->local_position.y == 1.0f,
                            "managed LateUpdate runs only after all active Behaviour Updates complete");

                        late_system.clear(
                            &late_world);

                        check(
                            managed_runtime.reset_time(),
                            "managed Time clock resets before multi-Behaviour host-frame probe");

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
                            "managed Time advances once per host Update frame regardless of Behaviour count");

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
                                disabled_tick.disabled == 1u &&
                                disabled_tick.destroyed == 0u &&
                                disabled_tick.updated == 0u &&
                                script_system.instance_count() == 1u &&
                                managed_runtime.instance_count() == 1,
                                "disabling ScriptBehaviour invokes OnDisable while preserving managed instance");

                            script_component->enabled =
                                true;

                            const auto reenabled_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                reenabled_tick.created == 0u &&
                                reenabled_tick.enabled == 1u &&
                                reenabled_tick.started == 0u &&
                                reenabled_tick.updated == 1u &&
                                managed_runtime.instance_count() == 1,
                                "reenabling ScriptBehaviour invokes OnEnable and reuses the already-started managed instance");

                            check(
                                native_transform &&
                                native_transform->local_position.x == 8.0f &&
                                native_transform->local_position.y == 6.0f &&
                                native_transform->local_position.z == 9.0f,
                                "reenabled managed Behaviour keeps state and receives current native Transform before Update");

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

                            const auto lifecycle_entity =
                                script_world.create(
                                    "Lifecycle Target");

                            auto* lifecycle_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        lifecycle_entity,
                                        script_behaviour_type());

                            if (lifecycle_script) {
                                lifecycle_script->type_name =
                                    "LifecycleProbe";
                            }

                            auto* lifecycle_transform =
                                script_world.transform(
                                    lifecycle_entity);

                            const auto lifecycle_first =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                lifecycle_script &&
                                lifecycle_first.created == 1u &&
                                lifecycle_first.awoken == 1u &&
                                lifecycle_first.enabled == 1u &&
                                lifecycle_first.started == 1u &&
                                lifecycle_first.updated == 1u &&
                                lifecycle_first.unresolved == 0u &&
                                lifecycle_transform &&
                                lifecycle_transform->local_position.x == 1111.0f,
                                "managed lifecycle runs Awake OnEnable Start Update in order on first activation");

                            lifecycle_script->enabled =
                                false;

                            const auto lifecycle_disabled =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                lifecycle_disabled.disabled == 1u &&
                                lifecycle_disabled.updated == 0u &&
                                lifecycle_disabled.destroyed == 0u &&
                                lifecycle_transform &&
                                lifecycle_transform->local_position.x == 11111.0f &&
                                script_system.instance_count() == 1u,
                                "native ScriptBehaviour disable invokes OnDisable without destroying managed instance");

                            lifecycle_script->enabled =
                                true;

                            const auto lifecycle_reenabled =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                lifecycle_reenabled.enabled == 1u &&
                                lifecycle_reenabled.started == 0u &&
                                lifecycle_reenabled.updated == 1u &&
                                lifecycle_transform &&
                                lifecycle_transform->local_position.x == 12121.0f &&
                                script_system.instance_count() == 1u,
                                "reenable invokes OnEnable without repeating Awake or Start");

                            script_world.destroy(
                                lifecycle_entity);

                            const auto lifecycle_destroyed =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            bool lifecycle_destroy_marker = false;

                            for (const auto candidate :
                                 script_world.entities()) {
                                if (script_world.name(candidate) ==
                                    "Lifecycle Destroyed") {
                                    lifecycle_destroy_marker = true;
                                    script_world.destroy(candidate);
                                    break;
                                }
                            }

                            check(
                                lifecycle_destroyed.disabled == 1u &&
                                lifecycle_destroyed.destroyed == 1u &&
                                lifecycle_destroyed.unresolved == 0u &&
                                lifecycle_destroy_marker &&
                                script_system.instance_count() == 0u,
                                "entity removal invokes OnDisable then OnDestroy while World callbacks remain bound");

                            const auto self_disable_entity =
                                script_world.create(
                                    "Self Disable Target");

                            auto* self_disable_script =
                                script_world.add_component<
                                    ScriptBehaviour>(
                                        self_disable_entity,
                                        script_behaviour_type());

                            if (self_disable_script) {
                                self_disable_script->type_name =
                                    "DisableSelf";
                            }

                            const auto self_disable_tick =
                                script_system.update(
                                    script_world,
                                    1.0f / 60.0f,
                                    &system_error);

                            check(
                                self_disable_script &&
                                !self_disable_script->enabled &&
                                self_disable_tick.created == 1u &&
                                self_disable_tick.awoken == 1u &&
                                self_disable_tick.enabled == 1u &&
                                self_disable_tick.started == 1u &&
                                self_disable_tick.updated == 1u &&
                                self_disable_tick.disabled == 1u &&
                                script_world.name(self_disable_entity) ==
                                    "Self Disabled" &&
                                script_system.instance_count() == 1u,
                                "managed Behaviour.enabled false synchronizes to native ScriptBehaviour and invokes OnDisable");

                            script_world.destroy(
                                self_disable_entity);

                            const auto self_disable_cleanup =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            check(
                                self_disable_cleanup.destroyed == 1u &&
                                script_system.instance_count() == 0u,
                                "self-disabled Behaviour remains alive until its native entity is destroyed");

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
                                inactive_tick.destroyed == 0u &&
                                inactive_tick.updated == 0u &&
                                script_system.instance_count() == 1u &&
                                managed_runtime.instance_count() == 1,
                                "inactive native GameObject preserves disabled managed instance without Update");

                            script_world.destroy(
                                active_entity);

                            const auto inactive_cleanup =
                                script_system.update(
                                    script_world,
                                    0.0f,
                                    &system_error);

                            check(
                                inactive_cleanup.destroyed == 1u &&
                                script_system.instance_count() == 0u &&
                                managed_runtime.instance_count() == 0,
                                "destroying inactive ScriptBehaviour entity releases preserved managed instance");

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
                                "managed ABI v8 callbacks mutate parent GameObject Transform and hierarchy immediately in native World");

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
