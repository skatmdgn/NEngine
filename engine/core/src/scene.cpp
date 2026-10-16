#include "nengine/core/scene.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace nengine::core {
namespace {
constexpr std::size_t max_scene_bytes = 32u * 1024u * 1024u;
constexpr std::size_t max_scene_objects = 10000u;
constexpr std::size_t max_object_components = 128u;
constexpr std::size_t max_component_properties = 256u;


void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

bool write_property_value(
    const SerializedPropertyData& property,
    std::uint32_t scene_version,
    std::ostream& output,
    std::string* error) {

    const auto& value = property.value;

    if (property.kind == PropertyKind::EntityReference) {
        if (scene_version < 3) {
            set_error(
                error,
                "EntityReference component properties require Scene v3");
            return false;
        }

        if (const auto* local = std::get_if<std::uint64_t>(&value)) {
            output << "ENTITY_LOCAL " << *local;
            return true;
        }

        if (const auto* null_ref = std::get_if<std::int64_t>(&value);
            null_ref && *null_ref < 0) {
            output << "ENTITY_LOCAL -1";
            return true;
        }

        set_error(
            error,
            "EntityReference property was not remapped to a scene-local id");
        return false;
    }

    if (property.kind == PropertyKind::AssetReference) {
        if (const auto* asset =
                std::get_if<std::string>(&value)) {
            output
                << "ASSET "
                << std::quoted(*asset);
            return true;
        }

        set_error(
            error,
            "AssetReference property must be stored as a GUID string");
        return false;
    }

    if (std::holds_alternative<std::monostate>(value)) {
        output << "NIL";
        return true;
    }

    if (const auto* typed = std::get_if<bool>(&value)) {
        output << "BOOL " << (*typed ? 1 : 0);
        return true;
    }

    if (const auto* typed = std::get_if<std::int64_t>(&value)) {
        output << "I64 " << *typed;
        return true;
    }

    if (const auto* typed = std::get_if<std::uint64_t>(&value)) {
        output << "U64 " << *typed;
        return true;
    }

    if (const auto* typed = std::get_if<double>(&value)) {
        output << "F64 "
               << std::setprecision(17)
               << *typed;
        return true;
    }

    if (const auto* typed = std::get_if<std::string>(&value)) {
        output << "STRING "
               << std::quoted(*typed);
        return true;
    }

    if (const auto* typed = std::get_if<Vec3>(&value)) {
        output << "VEC3 "
               << std::setprecision(9)
               << typed->x << ' '
               << typed->y << ' '
               << typed->z;
        return true;
    }

    if (const auto* typed = std::get_if<Quat>(&value)) {
        output << "QUAT "
               << std::setprecision(9)
               << typed->x << ' '
               << typed->y << ' '
               << typed->z << ' '
               << typed->w;
        return true;
    }

    if (std::holds_alternative<Entity>(value)) {
        set_error(
            error,
            "runtime Entity leaked into serialized property data");
        return false;
    }

    set_error(
        error,
        "unsupported serialized property value");
    return false;
}

bool read_property_value(
    std::istream& input,
    SerializedPropertyData& property,
    std::string* error) {

    auto& value = property.value;

    std::string tag;
    if (!(input >> tag)) {
        set_error(
            error,
            "missing property value type");
        return false;
    }

    if (tag == "NIL") {
        value = std::monostate{};
        return true;
    }

    if (tag == "BOOL") {
        int raw = 0;
        if (!(input >> raw)) {
            set_error(error, "malformed BOOL property");
            return false;
        }
        property.kind = PropertyKind::Boolean;
        value = raw != 0;
        return true;
    }

    if (tag == "I64") {
        std::int64_t raw = 0;
        if (!(input >> raw)) {
            set_error(error, "malformed I64 property");
            return false;
        }
        property.kind = PropertyKind::Integer;
        value = raw;
        return true;
    }

    if (tag == "U64") {
        std::uint64_t raw = 0;
        if (!(input >> raw)) {
            set_error(error, "malformed U64 property");
            return false;
        }
        property.kind = PropertyKind::UnsignedInteger;
        value = raw;
        return true;
    }

    if (tag == "F64") {
        double raw = 0.0;
        if (!(input >> raw)) {
            set_error(error, "malformed F64 property");
            return false;
        }
        property.kind = PropertyKind::Float;
        value = raw;
        return true;
    }

    if (tag == "STRING") {
        std::string raw;
        if (!(input >> std::quoted(raw))) {
            set_error(error, "malformed STRING property");
            return false;
        }
        property.kind = PropertyKind::String;
        value = std::move(raw);
        return true;
    }

    if (tag == "ASSET") {
        std::string raw;
        if (!(input >> std::quoted(raw))) {
            set_error(error, "malformed ASSET property");
            return false;
        }
        property.kind =
            PropertyKind::AssetReference;
        value = std::move(raw);
        return true;
    }

    if (tag == "VEC3") {
        Vec3 raw{};
        if (!(input >> raw.x >> raw.y >> raw.z)) {
            set_error(error, "malformed VEC3 property");
            return false;
        }
        property.kind = PropertyKind::Vec3;
        value = raw;
        return true;
    }

    if (tag == "QUAT") {
        Quat raw{};
        if (!(input >> raw.x >> raw.y >> raw.z >> raw.w)) {
            set_error(error, "malformed QUAT property");
            return false;
        }
        property.kind = PropertyKind::Quaternion;
        value = raw;
        return true;
    }

    if (tag == "ENTITY_LOCAL") {
        std::int64_t local = -1;
        if (!(input >> local)) {
            set_error(error, "malformed ENTITY_LOCAL property");
            return false;
        }

        property.kind = PropertyKind::EntityReference;

        if (local < 0) {
            value = std::int64_t{-1};
        } else {
            value = static_cast<std::uint64_t>(local);
        }
        return true;
    }

    set_error(
        error,
        "unknown serialized property value type: " +
            tag);
    return false;
}

bool write_component(
    const SerializedComponentData& component,
    std::uint32_t scene_version,
    std::ostream& output,
    std::string* error) {

    output
        << "COMPONENT "
        << component.type << ' '
        << component.version << ' '
        << std::quoted(component.type_name) << ' '
        << component.properties.size()
        << '\n';

    for (const auto& property :
         component.properties) {

        output
            << "PROPERTY "
            << std::quoted(property.name)
            << ' ';

        if (!write_property_value(
                property,
                scene_version,
                output,
                error)) {
            return false;
        }

        output << '\n';
    }

    output << "END_COMPONENT\n";
    return output.good();
}

bool read_component(
    std::istream& input,
    SerializedComponentData& component,
    std::string* error) {

    std::string token;
    std::size_t property_count = 0;

    if (!(input >> token) ||
        token != "COMPONENT" ||
        !(input
            >> component.type
            >> component.version
            >> std::quoted(component.type_name)
            >> property_count)) {

        set_error(
            error,
            "malformed component header");
        return false;
    }

    if (property_count > max_component_properties) {
        set_error(error, "scene component exceeds property limit");
        return false;
    }
    component.properties.clear();
    component.properties.reserve(property_count);

    for (std::size_t i = 0;
         i < property_count;
         ++i) {

        SerializedPropertyData property;

        if (!(input >> token) ||
            token != "PROPERTY" ||
            !(input >> std::quoted(property.name))) {

            set_error(
                error,
                "malformed component property header");
            return false;
        }

        if (!read_property_value(
                input,
                property,
                error)) {
            return false;
        }

        component.properties.push_back(
            std::move(property));
    }

    if (!(input >> token) ||
        token != "END_COMPONENT") {

        set_error(
            error,
            "missing component terminator");
        return false;
    }

    return true;
}

} // namespace

SceneData SceneSerializer::capture(
    const World& world,
    std::string scene_name,
    const ComponentSerializationRegistry* components) {

    SceneData scene;
    scene.name = std::move(scene_name);

    const auto entities =
        world.entities();

    scene.objects.reserve(
        entities.size());

    std::unordered_map<
        Entity::value_type,
        std::uint64_t> local_ids;

    local_ids.reserve(
        entities.size());

    for (std::uint64_t i = 0;
         i < entities.size();
         ++i) {

        local_ids.emplace(
            entities[
                static_cast<std::size_t>(i)]
                .value,
            i);
    }

    for (std::uint64_t i = 0;
         i < entities.size();
         ++i) {

        const auto entity =
            entities[
                static_cast<std::size_t>(i)];

        const auto view =
            world.view(entity);

        if (!view) {
            continue;
        }

        SceneObjectData object;
        object.local_id = i;
        object.name =
            std::string{view->name};
        object.active = view->active;
        object.transform = view->transform;
        object.parent_local_id = -1;

        if (object.transform.parent.valid()) {
            const auto parent_it =
                local_ids.find(
                    object.transform
                        .parent.value);

            if (parent_it !=
                local_ids.end()) {

                object.parent_local_id =
                    static_cast<std::int64_t>(
                        parent_it->second);
            }
        }

        object.transform.parent =
            Entity::invalid();

        if (components) {
            for (const auto type :
                 world.component_types(entity)) {

                if (type ==
                    World::transform_type) {
                    continue;
                }

                if (auto data =
                        components->capture(
                            world,
                            entity,
                            type)) {

                    for (auto& property :
                         data->properties) {

                        if (auto* reference =
                                std::get_if<Entity>(
                                    &property.value)) {

                            property.kind =
                                PropertyKind::EntityReference;

                            if (!reference->valid()) {
                                property.value =
                                    std::int64_t{-1};
                                continue;
                            }

                            const auto local =
                                local_ids.find(
                                    reference->value);

                            if (local ==
                                local_ids.end()) {
                                property.value =
                                    std::int64_t{-1};
                            } else {
                                property.value =
                                    local->second;
                            }
                        }
                    }

                    object.components.push_back(
                        std::move(*data));
                }
            }
        }

        scene.objects.push_back(
            std::move(object));
    }

    return scene;
}

bool SceneSerializer::instantiate(
    const SceneData& scene,
    World& destination,
    std::string* error,
    const ComponentSerializationRegistry* components) {

    if (scene.version >
        SceneData::current_version) {

        set_error(
            error,
            "scene version is newer than this engine");
        return false;
    }

    World staged;

    std::unordered_map<
        std::uint64_t,
        Entity> local_to_entity;

    local_to_entity.reserve(
        scene.objects.size());

    for (const auto& object :
         scene.objects) {

        if (local_to_entity.contains(
                object.local_id)) {

            set_error(
                error,
                "duplicate scene object local id");
            return false;
        }

        const auto entity =
            staged.create(object.name);

        staged.set_active(
            entity,
            object.active);

        if (auto* transform =
                staged.transform(entity)) {

            *transform =
                object.transform;

            transform->parent =
                Entity::invalid();
        }

        local_to_entity.emplace(
            object.local_id,
            entity);
    }

    for (const auto& object :
         scene.objects) {

        if (object.parent_local_id < 0) {
            continue;
        }

        const auto child_it =
            local_to_entity.find(
                object.local_id);

        const auto parent_it =
            local_to_entity.find(
                static_cast<std::uint64_t>(
                    object.parent_local_id));

        if (child_it ==
                local_to_entity.end() ||
            parent_it ==
                local_to_entity.end()) {

            set_error(
                error,
                "scene contains an invalid parent reference");
            return false;
        }

        if (!staged.set_parent(
                child_it->second,
                parent_it->second)) {

            set_error(
                error,
                "scene contains a cyclic or invalid hierarchy");
            return false;
        }
    }

    for (const auto& object :
         scene.objects) {

        if (object.components.empty()) {
            continue;
        }

        if (!components) {
            set_error(
                error,
                "scene contains serialized components but no component serialization registry was provided");
            return false;
        }

        const auto entity_it =
            local_to_entity.find(
                object.local_id);

        if (entity_it ==
            local_to_entity.end()) {

            set_error(
                error,
                "serialized component owner does not exist");
            return false;
        }

        for (const auto& component :
             object.components) {

            auto remapped = component;

            for (auto& property :
                 remapped.properties) {

                if (property.kind !=
                    PropertyKind::EntityReference) {
                    continue;
                }

                if (const auto* local =
                        std::get_if<std::uint64_t>(
                            &property.value)) {

                    const auto target =
                        local_to_entity.find(*local);

                    if (target ==
                        local_to_entity.end()) {
                        set_error(
                            error,
                            "EntityReference points to a missing scene-local object");
                        return false;
                    }

                    property.value =
                        target->second;
                    continue;
                }

                if (const auto* null_ref =
                        std::get_if<std::int64_t>(
                            &property.value);
                    null_ref && *null_ref < 0) {

                    property.value =
                        Entity::invalid();
                    continue;
                }

                set_error(
                    error,
                    "malformed EntityReference property");
                return false;
            }

            if (!components->restore(
                    staged,
                    entity_it->second,
                    remapped,
                    error)) {
                return false;
            }
        }
    }

    destination = std::move(staged);
    return true;
}

bool SceneSerializer::write(
    const SceneData& scene,
    std::ostream& output,
    std::string* error) {

    output
        << "NENGINE_SCENE "
        << scene.version
        << '\n';

    output
        << "NAME "
        << std::quoted(scene.name)
        << '\n';

    output
        << "OBJECTS "
        << scene.objects.size()
        << '\n';

    output << std::setprecision(9);

    for (const auto& object :
         scene.objects) {

        const auto& transform =
            object.transform;

        output
            << "OBJECT "
            << object.local_id << ' '
            << object.parent_local_id << ' '
            << (object.active ? 1 : 0)
            << ' '
            << std::quoted(object.name)
            << '\n';

        output
            << "POS "
            << transform.local_position.x << ' '
            << transform.local_position.y << ' '
            << transform.local_position.z
            << '\n';

        output
            << "ROT "
            << transform.local_rotation.x << ' '
            << transform.local_rotation.y << ' '
            << transform.local_rotation.z << ' '
            << transform.local_rotation.w
            << '\n';

        output
            << "SCALE "
            << transform.local_scale.x << ' '
            << transform.local_scale.y << ' '
            << transform.local_scale.z
            << '\n';

        if (scene.version >= 2) {
            output
                << "COMPONENTS "
                << object.components.size()
                << '\n';

            for (const auto& component :
                 object.components) {

                if (!write_component(
                        component,
                        scene.version,
                        output,
                        error)) {
                    return false;
                }
            }
        }

        output << "END_OBJECT\n";
    }

    output << "END_SCENE\n";

    if (!output.good()) {
        set_error(
            error,
            "failed while writing scene stream");
        return false;
    }

    return true;
}

static bool read_scene_records(
    std::istream& input,
    SceneData& scene,
    std::string* error) {

    SceneData parsed;
    std::string token;

    if (!(input >> token) ||
        token != "NENGINE_SCENE" ||
        !(input >> parsed.version)) {

        set_error(
            error,
            "invalid scene header");
        return false;
    }

    if (parsed.version < 1u ||
        parsed.version > SceneData::current_version) {

        set_error(
            error,
            "scene version is newer than this engine");
        return false;
    }

    if (!(input >> token) ||
        token != "NAME" ||
        !(input >> std::quoted(parsed.name))) {

        set_error(
            error,
            "missing scene name");
        return false;
    }

    std::size_t object_count = 0;

    if (!(input >> token) ||
        token != "OBJECTS" ||
        !(input >> object_count)) {

        set_error(
            error,
            "missing object count");
        return false;
    }

    if (object_count > max_scene_objects) {
        set_error(error, "scene object count exceeds limit");
        return false;
    }
    parsed.objects.reserve(
        object_count);

    for (std::size_t i = 0;
         i < object_count;
         ++i) {

        SceneObjectData object;
        int active = 0;

        if (!(input >> token) ||
            token != "OBJECT" ||
            !(input
                >> object.local_id
                >> object.parent_local_id
                >> active
                >> std::quoted(object.name))) {

            set_error(
                error,
                "malformed object header");
            return false;
        }

        object.active =
            active != 0;

        if (!(input >> token) ||
            token != "POS" ||
            !(input
                >> object.transform.local_position.x
                >> object.transform.local_position.y
                >> object.transform.local_position.z)) {

            set_error(
                error,
                "malformed position");
            return false;
        }

        if (!(input >> token) ||
            token != "ROT" ||
            !(input
                >> object.transform.local_rotation.x
                >> object.transform.local_rotation.y
                >> object.transform.local_rotation.z
                >> object.transform.local_rotation.w)) {

            set_error(
                error,
                "malformed rotation");
            return false;
        }

        if (!(input >> token) ||
            token != "SCALE" ||
            !(input
                >> object.transform.local_scale.x
                >> object.transform.local_scale.y
                >> object.transform.local_scale.z)) {

            set_error(
                error,
                "malformed scale");
            return false;
        }

        if (parsed.version >= 2) {
            std::size_t component_count = 0;

            if (!(input >> token) ||
                token != "COMPONENTS" ||
                !(input >> component_count)) {

                set_error(
                    error,
                    "missing component count");
                return false;
            }

            if (component_count > max_object_components) {
                set_error(error, "scene components per object exceed limit");
                return false;
            }
            object.components.reserve(
                component_count);

            for (std::size_t component_index = 0;
                 component_index < component_count;
                 ++component_index) {

                SerializedComponentData component;

                if (!read_component(
                        input,
                        component,
                        error)) {
                    return false;
                }

                object.components.push_back(
                    std::move(component));
            }
        }

        if (!(input >> token) ||
            token != "END_OBJECT") {

            set_error(
                error,
                "missing object terminator");
            return false;
        }

        object.transform.parent =
            Entity::invalid();

        parsed.objects.push_back(
            std::move(object));
    }

    if (!(input >> token) || token != "END_SCENE") {
        set_error(error, "missing scene terminator");
        return false;
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        set_error(error, "unexpected data after scene terminator");
        return false;
    }

    scene = std::move(parsed);
    return true;
}

bool SceneSerializer::read(
    std::istream& input,
    SceneData& scene,
    std::string* error) {

    // Bound the stream before std::quoted or any declared record count
    // can allocate memory. This also protects non-file input streams.
    std::string bytes;
    std::array<char, 4096> buffer{};
    for (;;) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = static_cast<std::size_t>(input.gcount());
        if (read > max_scene_bytes - bytes.size()) {
            set_error(error, "scene input exceeds 32 MiB limit");
            return false;
        }
        bytes.append(buffer.data(), read);
        if (input.eof()) break;
        if (!input) {
            set_error(error, "failed reading scene input stream");
            return false;
        }
    }
    std::istringstream bounded(bytes);
    return read_scene_records(bounded, scene, error);
}

bool SceneSerializer::save_file(
    const SceneData& scene,
    const std::filesystem::path& path,
    std::string* error) {

    // Complete serialization before touching any destination file.
    std::ostringstream serialized;
    if (!write(scene, serialized, error)) return false;
    const auto data = serialized.str();
    if (data.size() > max_scene_bytes) {
        set_error(error, "scene output exceeds 32 MiB limit");
        return false;
    }

    static std::atomic<std::uint64_t> sequence{0};
    auto temporary = path;
    temporary += ".nengine-tmp-" +
        std::to_string(std::chrono::steady_clock::now()
            .time_since_epoch().count()) + "-" +
        std::to_string(++sequence);
    const auto cleanup = [&temporary] {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    };

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "could not open temporary scene file");
            return false;
        }
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        output.flush();
        if (!output) {
            output.close();
            cleanup();
            set_error(error, "failed to flush temporary scene file");
            return false;
        }
        output.close();
        if (!output) {
            cleanup();
            set_error(error, "failed to close temporary scene file");
            return false;
        }
    }

#if defined(_WIN32)
    const bool replaced = MoveFileExW(
        temporary.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!replaced) {
        cleanup();
        set_error(error, "could not atomically replace destination scene");
        return false;
    }
#else
    std::error_code rename_error;
    std::filesystem::rename(temporary, path, rename_error);
    if (rename_error) {
        cleanup();
        set_error(error, "could not atomically replace destination scene: " +
            rename_error.message());
        return false;
    }
#endif
    if (error) error->clear();
    return true;
}

bool SceneSerializer::load_file(
    const std::filesystem::path& path,
    SceneData& scene,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open scene file for reading");
        return false;
    }

    return read(
        input,
        scene,
        error);
}

} // namespace nengine::core
