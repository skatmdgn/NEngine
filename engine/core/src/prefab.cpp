#include "nengine/core/prefab.hpp"

#include <unordered_set>
#include <utility>

namespace nengine::core {
namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

bool contains_object(const PrefabData& prefab, std::uint64_t local_id) {
    for (const auto& object : prefab.template_scene.objects) {
        if (object.local_id == local_id) {
            return true;
        }
    }
    return false;
}

} // namespace

bool PrefabModel::validate(const PrefabData& prefab, std::string* error) {
    if (prefab.version > PrefabData::current_version) {
        set_error(error, "prefab version is newer than this engine");
        return false;
    }
    if (prefab.template_scene.objects.empty()) {
        set_error(error, "prefab must contain at least one object");
        return false;
    }

    std::unordered_set<std::uint64_t> ids;
    ids.reserve(prefab.template_scene.objects.size());
    for (const auto& object : prefab.template_scene.objects) {
        if (!ids.insert(object.local_id).second) {
            set_error(error, "prefab contains duplicate object local ids");
            return false;
        }
    }
    if (!ids.contains(prefab.root_local_id)) {
        set_error(error, "prefab root object does not exist");
        return false;
    }

    World validation_world;
    std::string scene_error;
    if (!SceneSerializer::instantiate(prefab.template_scene, validation_world, &scene_error)) {
        set_error(error, "invalid prefab scene: " + scene_error);
        return false;
    }
    return true;
}

bool PrefabModel::validate_override(const PrefabData& prefab, const PrefabOverridePatch& patch, std::string* error) {
    if (!contains_object(prefab, patch.object_local_id) && patch.operation != PrefabOverrideOperation::AddObject) {
        set_error(error, "override targets an object that is not part of the prefab");
        return false;
    }

    switch (patch.operation) {
    case PrefabOverrideOperation::SetProperty:
        if (patch.component_type == ComponentRegistry::invalid_type || patch.property_path.empty()) {
            set_error(error, "property override requires component type and property path");
            return false;
        }
        break;
    case PrefabOverrideOperation::AddComponent:
    case PrefabOverrideOperation::RemoveComponent:
        if (patch.component_type == ComponentRegistry::invalid_type) {
            set_error(error, "component override requires a component type");
            return false;
        }
        break;
    case PrefabOverrideOperation::AddObject:
    case PrefabOverrideOperation::RemoveObject:
    case PrefabOverrideOperation::ReparentObject:
        break;
    }
    return true;
}

} // namespace nengine::core
