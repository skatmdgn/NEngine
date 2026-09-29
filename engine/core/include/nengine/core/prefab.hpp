#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/property_value.hpp"
#include "nengine/core/scene.hpp"

namespace nengine::core {

enum class PrefabOverrideOperation : std::uint8_t {
    SetProperty,
    AddComponent,
    RemoveComponent,
    AddObject,
    RemoveObject,
    ReparentObject,
};

struct PrefabOverridePatch {
    PrefabOverrideOperation operation{PrefabOverrideOperation::SetProperty};
    std::uint64_t object_local_id{};
    ComponentTypeId component_type{ComponentRegistry::invalid_type};
    std::string property_path{};
    PropertyValue value{};
};

struct PrefabData {
    static constexpr std::uint32_t current_version = 1;

    std::uint32_t version{current_version};
    std::string name{"Prefab"};
    SceneData template_scene{};
    std::uint64_t root_local_id{};
};

struct PrefabInstanceData {
    std::string source_asset{};
    std::vector<PrefabOverridePatch> overrides{};
};

class PrefabModel {
public:
    static bool validate(const PrefabData& prefab, std::string* error = nullptr);
    static bool validate_override(const PrefabData& prefab, const PrefabOverridePatch& patch, std::string* error = nullptr);
};

} // namespace nengine::core
