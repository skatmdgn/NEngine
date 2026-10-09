#include "nengine/editor/render_integration.hpp"

#include <cstdint>
#include <string>
#include <variant>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"

namespace nengine::editor {
namespace {

template <typename Component, typename Getter>
std::optional<core::PropertyValue> read_component_property(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    Getter getter) {

    const auto* component =
        world.get_component<Component>(
            entity,
            type);

    if (!component) {
        return std::nullopt;
    }

    return getter(*component);
}

template <typename Component, typename Setter>
bool write_component_property(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::PropertyValue& value,
    Setter setter) {

    auto* component =
        world.get_component<Component>(
            entity,
            type);

    return component &&
        setter(*component, value);
}

bool parse_asset_reference(
    const core::PropertyValue& value,
    assets::AssetGuid& destination) {

    const auto* text =
        std::get_if<std::string>(
            &value);

    if (!text) return false;

    if (text->empty()) {
        destination = {};
        return true;
    }

    const auto parsed =
        assets::AssetGuid::parse(*text);

    if (!parsed) return false;

    destination = *parsed;
    return true;
}

std::string asset_reference_text(
    assets::AssetGuid value) {

    return value.valid()
        ? value.to_string()
        : std::string{};
}

} // namespace

bool register_render_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry& serialization,
    PropertyAccessRegistry& properties) {

    bool ok = true;

    ok =
        render::register_component_metadata(
            components) &&
        ok;

    ok =
        render::register_component_serializers(
            serialization) &&
        ok;

    const auto camera_type =
        render::camera_type();

    ok =
        properties.register_property(
            camera_type,
            "Enabled",
            core::PropertyKind::Boolean,
            [camera_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Camera>(
                        world,
                        entity,
                        camera_type,
                        [](const render::Camera& value) {
                            return core::PropertyValue{
                                value.enabled};
                        });
            },
            [camera_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Camera>(
                        world,
                        entity,
                        camera_type,
                        value,
                        [](render::Camera& camera,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<bool>(&raw);
                            if (!typed) return false;
                            camera.enabled = *typed;
                            return true;
                        });
            }) && ok;

    ok =
        properties.register_property(
            camera_type,
            "Projection",
            core::PropertyKind::Integer,
            [camera_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Camera>(
                        world,
                        entity,
                        camera_type,
                        [](const render::Camera& value) {
                            return core::PropertyValue{
                                static_cast<std::int64_t>(
                                    value.projection)};
                        });
            },
            [camera_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Camera>(
                        world,
                        entity,
                        camera_type,
                        value,
                        [](render::Camera& camera,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<std::int64_t>(&raw);
                            if (!typed ||
                                *typed < 0 ||
                                *typed > static_cast<std::int64_t>(
                                    render::ProjectionMode::Orthographic)) {
                                return false;
                            }
                            camera.projection =
                                static_cast<render::ProjectionMode>(
                                    *typed);
                            return true;
                        });
            }) && ok;

    auto register_camera_float =
        [&properties, camera_type, &ok](
            const char* name,
            auto member) {

            ok =
                properties.register_property(
                    camera_type,
                    name,
                    core::PropertyKind::Float,
                    [camera_type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            render::Camera>(
                                world,
                                entity,
                                camera_type,
                                [member](
                                    const render::Camera& camera) {
                                    return core::PropertyValue{
                                        static_cast<double>(
                                            camera.*member)};
                                });
                    },
                    [camera_type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            render::Camera>(
                                world,
                                entity,
                                camera_type,
                                value,
                                [member](
                                    render::Camera& camera,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<double>(&raw);
                                    if (!typed) return false;
                                    camera.*member =
                                        static_cast<float>(*typed);
                                    return true;
                                });
                    }) &&
                ok;
        };

    register_camera_float(
        "Vertical FOV",
        &render::Camera::vertical_fov_degrees);

    register_camera_float(
        "Near Clip",
        &render::Camera::near_clip);

    register_camera_float(
        "Far Clip",
        &render::Camera::far_clip);

    register_camera_float(
        "Orthographic Size",
        &render::Camera::orthographic_size);

    const auto light_type =
        render::light_type();

    ok =
        properties.register_property(
            light_type,
            "Enabled",
            core::PropertyKind::Boolean,
            [light_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        [](const render::Light& value) {
                            return core::PropertyValue{
                                value.enabled};
                        });
            },
            [light_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        value,
                        [](render::Light& light,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<bool>(&raw);
                            if (!typed) return false;
                            light.enabled = *typed;
                            return true;
                        });
            }) && ok;

    ok =
        properties.register_property(
            light_type,
            "Type",
            core::PropertyKind::Integer,
            [light_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        [](const render::Light& value) {
                            return core::PropertyValue{
                                static_cast<std::int64_t>(
                                    value.type)};
                        });
            },
            [light_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        value,
                        [](render::Light& light,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<std::int64_t>(&raw);
                            if (!typed ||
                                *typed < 0 ||
                                *typed > static_cast<std::int64_t>(
                                    render::LightType::Spot)) {
                                return false;
                            }
                            light.type =
                                static_cast<render::LightType>(
                                    *typed);
                            return true;
                        });
            }) && ok;

    ok =
        properties.register_property(
            light_type,
            "Color",
            core::PropertyKind::Vec3,
            [light_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        [](const render::Light& value) {
                            return core::PropertyValue{
                                value.color};
                        });
            },
            [light_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        value,
                        [](render::Light& light,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<core::Vec3>(&raw);
                            if (!typed) return false;
                            light.color = *typed;
                            return true;
                        });
            }) && ok;

    auto register_light_float =
        [&properties, light_type, &ok](
            const char* name,
            auto member) {

            ok =
                properties.register_property(
                    light_type,
                    name,
                    core::PropertyKind::Float,
                    [light_type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            render::Light>(
                                world,
                                entity,
                                light_type,
                                [member](
                                    const render::Light& light) {
                                    return core::PropertyValue{
                                        static_cast<double>(
                                            light.*member)};
                                });
                    },
                    [light_type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            render::Light>(
                                world,
                                entity,
                                light_type,
                                value,
                                [member](
                                    render::Light& light,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<double>(&raw);
                                    if (!typed) return false;
                                    light.*member =
                                        static_cast<float>(*typed);
                                    return true;
                                });
                    }) &&
                ok;
        };

    register_light_float(
        "Intensity",
        &render::Light::intensity);

    register_light_float(
        "Range",
        &render::Light::range);

    register_light_float(
        "Spot Angle",
        &render::Light::spot_angle_degrees);

    ok =
        properties.register_property(
            light_type,
            "Cast Shadows",
            core::PropertyKind::Boolean,
            [light_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        [](const render::Light& value) {
                            return core::PropertyValue{
                                value.cast_shadows};
                        });
            },
            [light_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::Light>(
                        world,
                        entity,
                        light_type,
                        value,
                        [](render::Light& light,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<bool>(&raw);
                            if (!typed) return false;
                            light.cast_shadows = *typed;
                            return true;
                        });
            }) && ok;

    const auto mesh_type =
        render::mesh_renderer_type();

    auto register_mesh_bool =
        [&properties, mesh_type, &ok](
            const char* name,
            auto member) {

            ok =
                properties.register_property(
                    mesh_type,
                    name,
                    core::PropertyKind::Boolean,
                    [mesh_type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            render::MeshRenderer>(
                                world,
                                entity,
                                mesh_type,
                                [member](
                                    const render::MeshRenderer& renderer) {
                                    return core::PropertyValue{
                                        renderer.*member};
                                });
                    },
                    [mesh_type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            render::MeshRenderer>(
                                world,
                                entity,
                                mesh_type,
                                value,
                                [member](
                                    render::MeshRenderer& renderer,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<bool>(&raw);
                                    if (!typed) return false;
                                    renderer.*member = *typed;
                                    return true;
                                });
                    }) &&
                ok;
        };

    register_mesh_bool(
        "Enabled",
        &render::MeshRenderer::enabled);

    auto register_mesh_asset =
        [&properties, mesh_type, &ok](
            const char* name,
            auto member) {

            ok =
                properties.register_property(
                    mesh_type,
                    name,
                    core::PropertyKind::AssetReference,
                    [mesh_type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            render::MeshRenderer>(
                                world,
                                entity,
                                mesh_type,
                                [member](
                                    const render::MeshRenderer& renderer) {
                                    return core::PropertyValue{
                                        asset_reference_text(
                                            renderer.*member)};
                                });
                    },
                    [mesh_type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            render::MeshRenderer>(
                                world,
                                entity,
                                mesh_type,
                                value,
                                [member](
                                    render::MeshRenderer& renderer,
                                    const core::PropertyValue& raw) {
                                    assets::AssetGuid parsed;
                                    if (!parse_asset_reference(
                                            raw,
                                            parsed)) {
                                        return false;
                                    }
                                    renderer.*member =
                                        parsed;
                                    return true;
                                });
                    }) &&
                ok;
        };

    register_mesh_asset(
        "Mesh",
        &render::MeshRenderer::mesh);

    register_mesh_asset(
        "Material",
        &render::MeshRenderer::material);

    register_mesh_bool(
        "Cast Shadows",
        &render::MeshRenderer::cast_shadows);

    register_mesh_bool(
        "Receive Shadows",
        &render::MeshRenderer::receive_shadows);

    const auto sprite_type =
        render::sprite_renderer_type();

    auto register_sprite_bool =
        [&properties, sprite_type, &ok](
            const char* name,
            auto member) {

            ok =
                properties.register_property(
                    sprite_type,
                    name,
                    core::PropertyKind::Boolean,
                    [sprite_type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            render::SpriteRenderer>(
                                world,
                                entity,
                                sprite_type,
                                [member](
                                    const render::SpriteRenderer& renderer) {
                                    return core::PropertyValue{
                                        renderer.*member};
                                });
                    },
                    [sprite_type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            render::SpriteRenderer>(
                                world,
                                entity,
                                sprite_type,
                                value,
                                [member](
                                    render::SpriteRenderer& renderer,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<bool>(&raw);
                                    if (!typed) return false;
                                    renderer.*member =
                                        *typed;
                                    return true;
                                });
                    }) &&
                ok;
        };

    register_sprite_bool(
        "Enabled",
        &render::SpriteRenderer::enabled);

    register_sprite_bool(
        "Flip X",
        &render::SpriteRenderer::flip_x);

    register_sprite_bool(
        "Flip Y",
        &render::SpriteRenderer::flip_y);

    ok =
        properties.register_property(
            sprite_type,
            "Texture",
            core::PropertyKind::AssetReference,
            [sprite_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        [](const render::SpriteRenderer& renderer) {
                            return core::PropertyValue{
                                asset_reference_text(
                                    renderer.texture)};
                        });
            },
            [sprite_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        value,
                        [](render::SpriteRenderer& renderer,
                           const core::PropertyValue& raw) {
                            assets::AssetGuid parsed;
                            if (!parse_asset_reference(
                                    raw,
                                    parsed)) {
                                return false;
                            }
                            renderer.texture =
                                parsed;
                            return true;
                        });
            }) && ok;

    ok =
        properties.register_property(
            sprite_type,
            "Pixels Per Unit",
            core::PropertyKind::Float,
            [sprite_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        [](const render::SpriteRenderer& renderer) {
                            return core::PropertyValue{
                                static_cast<double>(
                                    renderer.pixels_per_unit)};
                        });
            },
            [sprite_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        value,
                        [](render::SpriteRenderer& renderer,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<double>(&raw);
                            if (!typed ||
                                *typed <= 0.0) {
                                return false;
                            }
                            renderer.pixels_per_unit =
                                static_cast<float>(
                                    *typed);
                            return true;
                        });
            }) && ok;

    ok =
        properties.register_property(
            sprite_type,
            "Sort Order",
            core::PropertyKind::Integer,
            [sprite_type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        [](const render::SpriteRenderer& renderer) {
                            return core::PropertyValue{
                                renderer.sort_order};
                        });
            },
            [sprite_type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    render::SpriteRenderer>(
                        world,
                        entity,
                        sprite_type,
                        value,
                        [](render::SpriteRenderer& renderer,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<std::int64_t>(&raw);
                            if (!typed) return false;
                            renderer.sort_order =
                                *typed;
                            return true;
                        });
            }) && ok;

    return ok;
}

} // namespace nengine::editor
