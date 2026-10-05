#include "nengine/core/component_serialization.hpp"

#include <utility>

namespace nengine::core {

bool ComponentSerializationRegistry::register_codec(
    Codec codec) {

    if (codec.type ==
            ComponentRegistry::invalid_type ||
        codec.type == World::transform_type ||
        codec.type_name.empty() ||
        codec.version == 0 ||
        !codec.capture ||
        !codec.restore ||
        codecs_.contains(codec.type)) {
        return false;
    }

    codecs_.emplace(
        codec.type,
        std::move(codec));

    return true;
}

const ComponentSerializationRegistry::Codec*
ComponentSerializationRegistry::find(
    ComponentTypeId type) const noexcept {

    const auto it = codecs_.find(type);
    return it == codecs_.end()
        ? nullptr
        : &it->second;
}

std::optional<SerializedComponentData>
ComponentSerializationRegistry::capture(
    const World& world,
    Entity entity,
    ComponentTypeId type) const {

    const auto* codec = find(type);
    if (!codec) return std::nullopt;

    auto data =
        codec->capture(world, entity);

    if (!data) return std::nullopt;

    data->type = codec->type;
    data->version = codec->version;

    if (data->type_name.empty()) {
        data->type_name = codec->type_name;
    }

    return data;
}

bool ComponentSerializationRegistry::restore(
    World& world,
    Entity entity,
    const SerializedComponentData& data,
    std::string* error) const {

    const auto* codec = find(data.type);

    if (!codec) {
        if (error) {
            *error =
                "no serializer registered for component: " +
                data.type_name;
        }
        return false;
    }

    if (data.version > codec->version) {
        if (error) {
            *error =
                "serialized component version is newer than registered codec";
        }
        return false;
    }

    return codec->restore(
        world,
        entity,
        data,
        error);
}

} // namespace nengine::core
