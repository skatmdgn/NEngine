#include "nengine/core/scene.hpp"

#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace nengine::core {
namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

} // namespace

SceneData SceneSerializer::capture(const World& world, std::string scene_name) {
    SceneData scene;
    scene.name = std::move(scene_name);

    const auto entities = world.entities();
    scene.objects.reserve(entities.size());

    std::unordered_map<Entity::value_type, std::uint64_t> local_ids;
    local_ids.reserve(entities.size());
    for (std::uint64_t i = 0; i < entities.size(); ++i) {
        local_ids.emplace(entities[static_cast<std::size_t>(i)].value, i);
    }

    for (std::uint64_t i = 0; i < entities.size(); ++i) {
        const auto entity = entities[static_cast<std::size_t>(i)];
        const auto view = world.view(entity);
        if (!view) {
            continue;
        }

        SceneObjectData object;
        object.local_id = i;
        object.name = std::string{view->name};
        object.active = view->active;
        object.transform = view->transform;
        object.parent_local_id = -1;

        if (object.transform.parent.valid()) {
            const auto parent_it = local_ids.find(object.transform.parent.value);
            if (parent_it != local_ids.end()) {
                object.parent_local_id = static_cast<std::int64_t>(parent_it->second);
            }
        }

        object.transform.parent = Entity::invalid();
        scene.objects.push_back(std::move(object));
    }
    return scene;
}

bool SceneSerializer::instantiate(const SceneData& scene, World& destination, std::string* error) {
    if (scene.version > SceneData::current_version) {
        set_error(error, "scene version is newer than this engine");
        return false;
    }

    World staged;
    std::unordered_map<std::uint64_t, Entity> local_to_entity;
    local_to_entity.reserve(scene.objects.size());

    for (const auto& object : scene.objects) {
        if (local_to_entity.contains(object.local_id)) {
            set_error(error, "duplicate scene object local id");
            return false;
        }
        const auto entity = staged.create(object.name);
        staged.set_active(entity, object.active);
        if (auto* transform = staged.transform(entity)) {
            *transform = object.transform;
            transform->parent = Entity::invalid();
        }
        local_to_entity.emplace(object.local_id, entity);
    }

    for (const auto& object : scene.objects) {
        if (object.parent_local_id < 0) {
            continue;
        }
        const auto child_it = local_to_entity.find(object.local_id);
        const auto parent_it = local_to_entity.find(static_cast<std::uint64_t>(object.parent_local_id));
        if (child_it == local_to_entity.end() || parent_it == local_to_entity.end()) {
            set_error(error, "scene contains an invalid parent reference");
            return false;
        }
        if (!staged.set_parent(child_it->second, parent_it->second)) {
            set_error(error, "scene contains a cyclic or invalid hierarchy");
            return false;
        }
    }

    destination = std::move(staged);
    return true;
}

bool SceneSerializer::write(const SceneData& scene, std::ostream& output, std::string* error) {
    output << "NENGINE_SCENE " << scene.version << '\n';
    output << "NAME " << std::quoted(scene.name) << '\n';
    output << "OBJECTS " << scene.objects.size() << '\n';
    output << std::setprecision(9);

    for (const auto& object : scene.objects) {
        const auto& t = object.transform;
        output << "OBJECT " << object.local_id << ' ' << object.parent_local_id << ' '
               << (object.active ? 1 : 0) << ' ' << std::quoted(object.name) << '\n';
        output << "POS " << t.local_position.x << ' ' << t.local_position.y << ' ' << t.local_position.z << '\n';
        output << "ROT " << t.local_rotation.x << ' ' << t.local_rotation.y << ' ' << t.local_rotation.z << ' ' << t.local_rotation.w << '\n';
        output << "SCALE " << t.local_scale.x << ' ' << t.local_scale.y << ' ' << t.local_scale.z << '\n';
        output << "END_OBJECT\n";
    }
    output << "END_SCENE\n";

    if (!output.good()) {
        set_error(error, "failed while writing scene stream");
        return false;
    }
    return true;
}

bool SceneSerializer::read(std::istream& input, SceneData& scene, std::string* error) {
    SceneData parsed;
    std::string token;

    if (!(input >> token) || token != "NENGINE_SCENE" || !(input >> parsed.version)) {
        set_error(error, "invalid scene header");
        return false;
    }
    if (parsed.version > SceneData::current_version) {
        set_error(error, "scene version is newer than this engine");
        return false;
    }

    if (!(input >> token) || token != "NAME" || !(input >> std::quoted(parsed.name))) {
        set_error(error, "missing scene name");
        return false;
    }

    std::size_t object_count = 0;
    if (!(input >> token) || token != "OBJECTS" || !(input >> object_count)) {
        set_error(error, "missing object count");
        return false;
    }

    parsed.objects.reserve(object_count);
    for (std::size_t i = 0; i < object_count; ++i) {
        SceneObjectData object;
        int active = 0;
        if (!(input >> token) || token != "OBJECT" ||
            !(input >> object.local_id >> object.parent_local_id >> active >> std::quoted(object.name))) {
            set_error(error, "malformed object header");
            return false;
        }
        object.active = active != 0;

        if (!(input >> token) || token != "POS" ||
            !(input >> object.transform.local_position.x >> object.transform.local_position.y >> object.transform.local_position.z)) {
            set_error(error, "malformed position");
            return false;
        }
        if (!(input >> token) || token != "ROT" ||
            !(input >> object.transform.local_rotation.x >> object.transform.local_rotation.y >>
              object.transform.local_rotation.z >> object.transform.local_rotation.w)) {
            set_error(error, "malformed rotation");
            return false;
        }
        if (!(input >> token) || token != "SCALE" ||
            !(input >> object.transform.local_scale.x >> object.transform.local_scale.y >> object.transform.local_scale.z)) {
            set_error(error, "malformed scale");
            return false;
        }
        if (!(input >> token) || token != "END_OBJECT") {
            set_error(error, "missing object terminator");
            return false;
        }
        object.transform.parent = Entity::invalid();
        parsed.objects.push_back(std::move(object));
    }

    if (!(input >> token) || token != "END_SCENE") {
        set_error(error, "missing scene terminator");
        return false;
    }

    scene = std::move(parsed);
    return true;
}

bool SceneSerializer::save_file(const SceneData& scene, const std::filesystem::path& path, std::string* error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        set_error(error, "could not open scene file for writing");
        return false;
    }
    return write(scene, output, error);
}

bool SceneSerializer::load_file(const std::filesystem::path& path, SceneData& scene, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        set_error(error, "could not open scene file for reading");
        return false;
    }
    return read(input, scene, error);
}

} // namespace nengine::core
