#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "nengine/core/component_serialization.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"

namespace nengine::core {

struct SceneObjectData {
    std::uint64_t local_id{};
    std::string name{};
    bool active{true};
    Transform transform{};
    std::int64_t parent_local_id{-1};
    std::vector<SerializedComponentData> components{};
};

struct SceneData {
    static constexpr std::uint32_t current_version = 3;

    std::uint32_t version{current_version};
    std::string name{"Untitled"};
    std::vector<SceneObjectData> objects{};
};

class SceneSerializer {
public:
    static SceneData capture(
        const World& world,
        std::string scene_name = "Untitled",
        const ComponentSerializationRegistry* components = nullptr);

    static bool instantiate(
        const SceneData& scene,
        World& destination,
        std::string* error = nullptr,
        const ComponentSerializationRegistry* components = nullptr);

    static bool write(
        const SceneData& scene,
        std::ostream& output,
        std::string* error = nullptr);

    static bool read(
        std::istream& input,
        SceneData& scene,
        std::string* error = nullptr);

    static bool save_file(
        const SceneData& scene,
        const std::filesystem::path& path,
        std::string* error = nullptr);

    static bool load_file(
        const std::filesystem::path& path,
        SceneData& scene,
        std::string* error = nullptr);
};

} // namespace nengine::core
