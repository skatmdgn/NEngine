#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"

namespace nengine::core {

struct SceneObjectData {
    std::uint64_t local_id{};
    std::string name{};
    bool active{true};
    Transform transform{};
    std::int64_t parent_local_id{-1};
};

struct SceneData {
    static constexpr std::uint32_t current_version = 1;

    std::uint32_t version{current_version};
    std::string name{"Untitled"};
    std::vector<SceneObjectData> objects{};
};

class SceneSerializer {
public:
    static SceneData capture(const World& world, std::string scene_name = "Untitled");
    static bool instantiate(const SceneData& scene, World& destination, std::string* error = nullptr);

    static bool write(const SceneData& scene, std::ostream& output, std::string* error = nullptr);
    static bool read(std::istream& input, SceneData& scene, std::string* error = nullptr);

    static bool save_file(const SceneData& scene, const std::string& path, std::string* error = nullptr);
    static bool load_file(const std::string& path, SceneData& scene, std::string* error = nullptr);
};

} // namespace nengine::core
