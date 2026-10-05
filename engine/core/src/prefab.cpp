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

    // Prefab structural validation must not depend on all
    // runtime component codecs being loaded. Validate hierarchy
    // with component payloads removed, then validate scene-local
    // EntityReference values against the prefab object set.
    SceneData structural_scene =
        prefab.template_scene;

    for (auto& object :
         structural_scene.objects) {
        object.components.clear();
    }

    World validation_world;
    std::string scene_error;

    if (!SceneSerializer::instantiate(
            structural_scene,
            validation_world,
            &scene_error)) {

        set_error(
            error,
            "invalid prefab scene: " +
                scene_error);
        return false;
    }

    for (const auto& object :
         prefab.template_scene.objects) {

        for (const auto& component :
             object.components) {

            for (const auto& property :
                 component.properties) {

                if (property.kind !=
                    PropertyKind::EntityReference) {
                    continue;
                }

                if (const auto* local =
                        std::get_if<std::uint64_t>(
                            &property.value)) {

                    if (!ids.contains(*local)) {
                        set_error(
                            error,
                            "prefab EntityReference targets a missing local object");
                        return false;
                    }

                    continue;
                }

                if (const auto* null_reference =
                        std::get_if<std::int64_t>(
                            &property.value);
                    null_reference &&
                    *null_reference < 0) {
                    continue;
                }

                set_error(
                    error,
                    "prefab contains a malformed EntityReference");
                return false;
            }
        }
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
