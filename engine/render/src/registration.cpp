#include "nengine/render/registration.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "nengine/render/components.hpp"

namespace nengine::render {
namespace {

const core::SerializedPropertyData* find_property(
    const core::SerializedComponentData& data,
    std::string_view name) {

    for (const auto& property :
         data.properties) {
        if (property.name == name) {
            return &property;
        }
    }

    return nullptr;
}

bool read_bool(
    const core::SerializedComponentData& data,
    std::string_view name,
    bool& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<bool>(
            &property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_i64(
    const core::SerializedComponentData& data,
    std::string_view name,
    std::int64_t& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<std::int64_t>(
            &property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_float(
    const core::SerializedComponentData& data,
    std::string_view name,
    float& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<double>(
            &property->value);

    if (!typed) return false;

    value =
        static_cast<float>(*typed);

    return true;
}

bool read_vec3(
    const core::SerializedComponentData& data,
    std::string_view name,
    core::Vec3& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<core::Vec3>(
            &property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_asset(
    const core::SerializedComponentData& data,
    std::string_view name,
    assets::AssetGuid& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<std::string>(
            &property->value);

    if (!typed) return false;

    if (typed->empty()) {
        value = {};
        return true;
    }

    const auto parsed =
        assets::AssetGuid::parse(
            *typed);

    if (!parsed) return false;

    value = *parsed;
    return true;
}

core::SerializedPropertyData boolean_property(
    std::string name,
    bool value) {

    return {
        std::move(name),
        core::PropertyKind::Boolean,
        core::PropertyValue{value}
    };
}

core::SerializedPropertyData integer_property(
    std::string name,
    std::int64_t value) {

    return {
        std::move(name),
        core::PropertyKind::Integer,
        core::PropertyValue{value}
    };
}

core::SerializedPropertyData float_property(
    std::string name,
    float value) {

    return {
        std::move(name),
        core::PropertyKind::Float,
        core::PropertyValue{
            static_cast<double>(value)}
    };
}

core::SerializedPropertyData vec3_property(
    std::string name,
    core::Vec3 value) {

    return {
        std::move(name),
        core::PropertyKind::Vec3,
        core::PropertyValue{value}
    };
}

core::SerializedPropertyData asset_property(
    std::string name,
    assets::AssetGuid value) {

    return {
        std::move(name),
        core::PropertyKind::AssetReference,
        core::PropertyValue{
            value.valid()
                ? value.to_string()
                : std::string{}}
    };
}

} // namespace

bool register_component_metadata(
    core::ComponentRegistry& registry) {

    bool ok = true;

    ok =
        registry.register_type(
            "NEngine.Camera",
            "Rendering",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Projection",
                core::PropertyKind::Integer,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Vertical FOV",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Near Clip",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Far Clip",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            camera_type(),
            {
                "Orthographic Size",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_type(
            "NEngine.Light",
            "Rendering",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Type",
                core::PropertyKind::Integer,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Color",
                core::PropertyKind::Vec3,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Intensity",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Range",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Spot Angle",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            light_type(),
            {
                "Cast Shadows",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_type(
            "NEngine.SpriteRenderer",
            "Rendering",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Texture",
                core::PropertyKind::AssetReference,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Pixels Per Unit",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Sort Order",
                core::PropertyKind::Integer,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Flip X",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_renderer_type(),
            {
                "Flip Y",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_type(
            "NEngine.SpriteAnimator",
            "Rendering",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            sprite_animator_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_animator_type(),
            {
                "Clip",
                core::PropertyKind::AssetReference,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_animator_type(),
            {
                "Playing",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_animator_type(),
            {
                "Loop",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            sprite_animator_type(),
            {
                "Speed",
                core::PropertyKind::Float,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_type(
            "NEngine.MeshRenderer",
            "Rendering",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            mesh_renderer_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            mesh_renderer_type(),
            {
                "Mesh",
                core::PropertyKind::AssetReference,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            mesh_renderer_type(),
            {
                "Material",
                core::PropertyKind::AssetReference,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            mesh_renderer_type(),
            {
                "Cast Shadows",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    ok =
        registry.register_property(
            mesh_renderer_type(),
            {
                "Receive Shadows",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) && ok;

    return ok;
}

bool register_component_serializers(
    core::ComponentSerializationRegistry& registry) {

    bool ok = true;

    ok =
        registry.register_codec({
            camera_type(),
            1,
            "NEngine.Camera",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* camera =
                    world.get_component<Camera>(
                        entity,
                        camera_type());

                if (!camera) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    boolean_property(
                        "Enabled",
                        camera->enabled),
                    integer_property(
                        "Projection",
                        static_cast<
                            std::int64_t>(
                                camera->projection)),
                    float_property(
                        "Vertical FOV",
                        camera->vertical_fov_degrees),
                    float_property(
                        "Near Clip",
                        camera->near_clip),
                    float_property(
                        "Far Clip",
                        camera->far_clip),
                    float_property(
                        "Orthographic Size",
                        camera->orthographic_size)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                Camera value;
                std::int64_t projection = 0;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_i64(
                        data,
                        "Projection",
                        projection) ||
                    !read_float(
                        data,
                        "Vertical FOV",
                        value.vertical_fov_degrees) ||
                    !read_float(
                        data,
                        "Near Clip",
                        value.near_clip) ||
                    !read_float(
                        data,
                        "Far Clip",
                        value.far_clip) ||
                    !read_float(
                        data,
                        "Orthographic Size",
                        value.orthographic_size)) {

                    if (error) {
                        *error =
                            "malformed NEngine.Camera data";
                    }
                    return false;
                }

                if (projection < 0 ||
                    projection >
                        static_cast<std::int64_t>(
                            ProjectionMode::Orthographic)) {

                    if (error) {
                        *error =
                            "invalid Camera projection mode";
                    }
                    return false;
                }

                value.projection =
                    static_cast<ProjectionMode>(
                        projection);

                auto* camera =
                    world.get_component<Camera>(
                        entity,
                        camera_type());

                if (!camera) {
                    camera =
                        world.add_component<Camera>(
                            entity,
                            camera_type());
                }

                if (!camera) return false;
                *camera = value;
                return true;
            }
        }) && ok;

    ok =
        registry.register_codec({
            light_type(),
            1,
            "NEngine.Light",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* light =
                    world.get_component<Light>(
                        entity,
                        light_type());

                if (!light) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    boolean_property(
                        "Enabled",
                        light->enabled),
                    integer_property(
                        "Type",
                        static_cast<std::int64_t>(
                            light->type)),
                    vec3_property(
                        "Color",
                        light->color),
                    float_property(
                        "Intensity",
                        light->intensity),
                    float_property(
                        "Range",
                        light->range),
                    float_property(
                        "Spot Angle",
                        light->spot_angle_degrees),
                    boolean_property(
                        "Cast Shadows",
                        light->cast_shadows)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                Light value;
                std::int64_t type = 0;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_i64(
                        data,
                        "Type",
                        type) ||
                    !read_vec3(
                        data,
                        "Color",
                        value.color) ||
                    !read_float(
                        data,
                        "Intensity",
                        value.intensity) ||
                    !read_float(
                        data,
                        "Range",
                        value.range) ||
                    !read_float(
                        data,
                        "Spot Angle",
                        value.spot_angle_degrees) ||
                    !read_bool(
                        data,
                        "Cast Shadows",
                        value.cast_shadows)) {

                    if (error) {
                        *error =
                            "malformed NEngine.Light data";
                    }
                    return false;
                }

                if (type < 0 ||
                    type >
                        static_cast<std::int64_t>(
                            LightType::Spot)) {
                    if (error) {
                        *error =
                            "invalid Light type";
                    }
                    return false;
                }

                value.type =
                    static_cast<LightType>(type);

                auto* light =
                    world.get_component<Light>(
                        entity,
                        light_type());

                if (!light) {
                    light =
                        world.add_component<Light>(
                            entity,
                            light_type());
                }

                if (!light) return false;
                *light = value;
                return true;
            }
        }) && ok;

    ok =
        registry.register_codec({
            sprite_renderer_type(),
            1,
            "NEngine.SpriteRenderer",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* renderer =
                    world.get_component<SpriteRenderer>(
                        entity,
                        sprite_renderer_type());

                if (!renderer) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    boolean_property(
                        "Enabled",
                        renderer->enabled),
                    asset_property(
                        "Texture",
                        renderer->texture),
                    float_property(
                        "Pixels Per Unit",
                        renderer->pixels_per_unit),
                    integer_property(
                        "Sort Order",
                        renderer->sort_order),
                    boolean_property(
                        "Flip X",
                        renderer->flip_x),
                    boolean_property(
                        "Flip Y",
                        renderer->flip_y)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                SpriteRenderer value;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_asset(
                        data,
                        "Texture",
                        value.texture) ||
                    !read_float(
                        data,
                        "Pixels Per Unit",
                        value.pixels_per_unit) ||
                    !read_i64(
                        data,
                        "Sort Order",
                        value.sort_order) ||
                    !read_bool(
                        data,
                        "Flip X",
                        value.flip_x) ||
                    !read_bool(
                        data,
                        "Flip Y",
                        value.flip_y) ||
                    value.pixels_per_unit <=
                        0.0f) {

                    if (error) {
                        *error =
                            "malformed NEngine.SpriteRenderer data";
                    }
                    return false;
                }

                auto* renderer =
                    world.get_component<SpriteRenderer>(
                        entity,
                        sprite_renderer_type());

                if (!renderer) {
                    renderer =
                        world.add_component<SpriteRenderer>(
                            entity,
                            sprite_renderer_type());
                }

                if (!renderer) return false;
                *renderer = value;
                return true;
            }
        }) && ok;

    ok =
        registry.register_codec({
            sprite_animator_type(),
            1,
            "NEngine.SpriteAnimator",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* animator =
                    world.get_component<SpriteAnimator>(
                        entity,
                        sprite_animator_type());

                if (!animator) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    boolean_property(
                        "Enabled",
                        animator->enabled),
                    asset_property(
                        "Clip",
                        animator->clip),
                    boolean_property(
                        "Playing",
                        animator->playing),
                    boolean_property(
                        "Loop",
                        animator->loop),
                    float_property(
                        "Speed",
                        animator->speed)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                SpriteAnimator value;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_asset(
                        data,
                        "Clip",
                        value.clip) ||
                    !read_bool(
                        data,
                        "Playing",
                        value.playing) ||
                    !read_bool(
                        data,
                        "Loop",
                        value.loop) ||
                    !read_float(
                        data,
                        "Speed",
                        value.speed) ||
                    value.speed < 0.0f) {

                    if (error) {
                        *error =
                            "malformed NEngine.SpriteAnimator data";
                    }
                    return false;
                }

                value.time_seconds = 0.0f;

                auto* animator =
                    world.get_component<SpriteAnimator>(
                        entity,
                        sprite_animator_type());

                if (!animator) {
                    animator =
                        world.add_component<SpriteAnimator>(
                            entity,
                            sprite_animator_type());
                }

                if (!animator) return false;
                *animator = value;
                return true;
            }
        }) && ok;

    ok =
        registry.register_codec({
            mesh_renderer_type(),
            1,
            "NEngine.MeshRenderer",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* renderer =
                    world.get_component<MeshRenderer>(
                        entity,
                        mesh_renderer_type());

                if (!renderer) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    boolean_property(
                        "Enabled",
                        renderer->enabled),
                    asset_property(
                        "Mesh",
                        renderer->mesh),
                    asset_property(
                        "Material",
                        renderer->material),
                    boolean_property(
                        "Cast Shadows",
                        renderer->cast_shadows),
                    boolean_property(
                        "Receive Shadows",
                        renderer->receive_shadows)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                MeshRenderer value;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_asset(
                        data,
                        "Mesh",
                        value.mesh) ||
                    !read_asset(
                        data,
                        "Material",
                        value.material) ||
                    !read_bool(
                        data,
                        "Cast Shadows",
                        value.cast_shadows) ||
                    !read_bool(
                        data,
                        "Receive Shadows",
                        value.receive_shadows)) {

                    if (error) {
                        *error =
                            "malformed NEngine.MeshRenderer data";
                    }
                    return false;
                }

                auto* renderer =
                    world.get_component<MeshRenderer>(
                        entity,
                        mesh_renderer_type());

                if (!renderer) {
                    renderer =
                        world.add_component<MeshRenderer>(
                            entity,
                            mesh_renderer_type());
                }

                if (!renderer) return false;
                *renderer = value;
                return true;
            }
        }) && ok;

    return ok;
}

} // namespace nengine::render
