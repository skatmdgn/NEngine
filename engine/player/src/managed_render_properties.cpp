#include "managed_render_properties.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/render/components.hpp"

namespace nengine::player {
namespace {
bool finite_color(const core::Vec3& c) {
    return std::isfinite(c.x) && std::isfinite(c.y) &&
        std::isfinite(c.z) && c.x >= 0.f && c.y >= 0.f && c.z >= 0.f;
}

bool bounded_float(const core::PropertyValue& value, float& out,
                   double lower, double upper, bool strict_lower = false) {
    const auto* v = std::get_if<double>(&value);
    if (!v || !std::isfinite(*v) ||
        (strict_lower ? *v <= lower : *v < lower) || *v > upper)
        return false;
    const float cast = static_cast<float>(*v);
    if (!std::isfinite(cast) || (strict_lower && cast <= lower))
        return false;
    out = cast;
    return true;
}

bool asset_value(const core::PropertyValue& value,
                 assets::AssetGuid& guid) {
    const auto* text = std::get_if<std::string>(&value);
    if (!text) return false;
    if (text->empty()) { guid = {}; return true; }
    const auto parsed = assets::AssetGuid::parse(*text);
    if (!parsed || !parsed->valid()) return false;
    guid = *parsed;
    return true;
}

std::string asset_text(assets::AssetGuid guid) {
    return guid.valid() ? guid.to_string() : std::string{};
}

template <typename Component>
bool bool_value(const core::PropertyValue& value,
                Component& component, bool Component::* member) {
    const auto* typed = std::get_if<bool>(&value);
    if (!typed) return false;
    component.*member = *typed;
    return true;
}

bool read_camera(const render::Camera& v, std::string_view key,
                 core::PropertyValue& out) {
    if (key == "Enabled") out = v.enabled;
    else if (key == "Projection")
        out = static_cast<std::int64_t>(v.projection);
    else if (key == "Vertical FOV")
        out = static_cast<double>(v.vertical_fov_degrees);
    else if (key == "Near Clip") out = static_cast<double>(v.near_clip);
    else if (key == "Far Clip") out = static_cast<double>(v.far_clip);
    else if (key == "Orthographic Size")
        out = static_cast<double>(v.orthographic_size);
    else return false;
    return true;
}

bool write_camera(render::Camera& c, std::string_view key,
                  const core::PropertyValue& value) {
    if (key == "Enabled")
        return bool_value(value, c, &render::Camera::enabled);
    if (key == "Projection") {
        const auto* n = std::get_if<std::int64_t>(&value);
        if (!n || *n < 0 || *n > 1) return false;
        c.projection = static_cast<render::ProjectionMode>(*n);
        return true;
    }
    float f{};
    if (key == "Vertical FOV") {
        if (!bounded_float(value, f, 0.0, 179.0, true)) return false;
        c.vertical_fov_degrees = f;
    } else if (key == "Near Clip") {
        if (!bounded_float(value, f, 0.0, c.far_clip, true) ||
            f >= c.far_clip) return false;
        c.near_clip = f;
    } else if (key == "Far Clip") {
        if (!bounded_float(value, f, c.near_clip, 1.0e9, true))
            return false;
        c.far_clip = f;
    } else if (key == "Orthographic Size") {
        if (!bounded_float(value, f, 0.0, 1.0e9, true)) return false;
        c.orthographic_size = f;
    } else return false;
    return true;
}

bool read_light(const render::Light& v, std::string_view key,
                core::PropertyValue& out) {
    if (key == "Enabled") out = v.enabled;
    else if (key == "Type") out = static_cast<std::int64_t>(v.type);
    else if (key == "Color") out = v.color;
    else if (key == "Intensity") out = static_cast<double>(v.intensity);
    else if (key == "Range") out = static_cast<double>(v.range);
    else if (key == "Spot Angle") out = static_cast<double>(v.spot_angle_degrees);
    else if (key == "Cast Shadows") out = v.cast_shadows;
    else return false;
    return true;
}

bool write_light(render::Light& v, std::string_view key,
                 const core::PropertyValue& value) {
    if (key == "Enabled")
        return bool_value(value, v, &render::Light::enabled);
    if (key == "Cast Shadows")
        return bool_value(value, v, &render::Light::cast_shadows);
    if (key == "Type") {
        const auto* n = std::get_if<std::int64_t>(&value);
        if (!n || *n < 0 || *n > 2) return false;
        v.type = static_cast<render::LightType>(*n);
        return true;
    }
    if (key == "Color") {
        const auto* c = std::get_if<core::Vec3>(&value);
        if (!c || !finite_color(*c)) return false;
        v.color = *c;
        return true;
    }
    float f{};
    if (key == "Intensity") {
        if (!bounded_float(value, f, 0.0, 1.0e6)) return false;
        v.intensity = f;
    } else if (key == "Range") {
        if (!bounded_float(value, f, 0.0, 1.0e9, true)) return false;
        v.range = f;
    } else if (key == "Spot Angle") {
        if (!bounded_float(value, f, 0.0, 180.0, true)) return false;
        v.spot_angle_degrees = f;
    } else return false;
    return true;
}

bool read_sprite(const render::SpriteRenderer& v, std::string_view key,
                 core::PropertyValue& out) {
    if (key == "Enabled") out = v.enabled;
    else if (key == "Texture") out = asset_text(v.texture);
    else if (key == "Pixels Per Unit")
        out = static_cast<double>(v.pixels_per_unit);
    else if (key == "Sort Order") out = v.sort_order;
    else if (key == "Flip X") out = v.flip_x;
    else if (key == "Flip Y") out = v.flip_y;
    else return false;
    return true;
}

bool write_sprite(render::SpriteRenderer& v, std::string_view key,
                  const core::PropertyValue& value) {
    if (key == "Enabled")
        return bool_value(value, v, &render::SpriteRenderer::enabled);
    if (key == "Flip X")
        return bool_value(value, v, &render::SpriteRenderer::flip_x);
    if (key == "Flip Y")
        return bool_value(value, v, &render::SpriteRenderer::flip_y);
    if (key == "Texture") {
        assets::AssetGuid guid;
        if (!asset_value(value, guid)) return false;
        v.texture = guid;
        return true;
    }
    if (key == "Sort Order") {
        const auto* n = std::get_if<std::int64_t>(&value);
        if (!n) return false;
        v.sort_order = *n;
        return true;
    }
    if (key == "Pixels Per Unit") {
        float f{};
        if (!bounded_float(value, f, 0.0, 1.0e6, true)) return false;
        v.pixels_per_unit = f;
        return true;
    }
    return false;
}


bool read_animator(const render::SpriteAnimator& a, std::string_view key,
                   core::PropertyValue& out) {
    if (key == "Enabled") out = a.enabled;
    else if (key == "Playing") out = a.playing;
    else if (key == "Loop") out = a.loop;
    else if (key == "Clip") out = asset_text(a.clip);
    else if (key == "Controller") out = asset_text(a.controller);
    else if (key == "Speed") out = static_cast<double>(a.speed);
    else if (key == "Time") out = static_cast<double>(a.time_seconds);
    else if (key == "Bool Parameter") out = a.last_bool_parameter;
    else return false;
    return true;
}

bool write_animator(render::SpriteAnimator& a, std::string_view key,
                    const core::PropertyValue& value) {
    if (key == "Enabled") return bool_value(value, a, &render::SpriteAnimator::enabled);
    if (key == "Playing") return bool_value(value, a, &render::SpriteAnimator::playing);
    if (key == "Loop") return bool_value(value, a, &render::SpriteAnimator::loop);
    if (key == "Clip" || key == "Controller") {
        assets::AssetGuid next;
        if (!asset_value(value, next)) return false;
        if (key == "Clip") a.clip = next;
        else a.controller = next;
        a.time_seconds = 0.f;
        return true;
    }
    float f{};
    if (key == "Speed" || key == "Time") {
        if (!bounded_float(value, f, 0.0, 1.0e6)) return false;
        if (key == "Speed") a.speed = f;
        else a.time_seconds = f;
        return true;
    }
    return false;
}

bool read_mesh(const render::MeshRenderer& v, std::string_view key,
               core::PropertyValue& out) {
    if (key == "Enabled") out = v.enabled;
    else if (key == "Cast Shadows") out = v.cast_shadows;
    else if (key == "Receive Shadows") out = v.receive_shadows;
    else if (key == "Mesh") out = asset_text(v.mesh);
    else if (key == "Material") out = asset_text(v.material);
    else return false;
    return true;
}

bool write_mesh(render::MeshRenderer& v, std::string_view key,
                const core::PropertyValue& value) {
    if (key == "Enabled")
        return bool_value(value, v, &render::MeshRenderer::enabled);
    if (key == "Cast Shadows")
        return bool_value(value, v, &render::MeshRenderer::cast_shadows);
    if (key == "Receive Shadows")
        return bool_value(value, v, &render::MeshRenderer::receive_shadows);
    if (key == "Mesh" || key == "Material") {
        assets::AssetGuid guid;
        if (!asset_value(value, guid)) return false;
        if (key == "Mesh") v.mesh = guid;
        else v.material = guid;
        return true;
    }
    return false;
}
} // namespace

bool read_player_render_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.Camera") {
        const auto* c = world.get_component<render::Camera>(
            entity, render::camera_type());
        return c && read_camera(*c, property, value);
    }
    if (component == "NEngine.Light") {
        const auto* c = world.get_component<render::Light>(
            entity, render::light_type());
        return c && read_light(*c, property, value);
    }
    if (component == "NEngine.SpriteRenderer") {
        const auto* c = world.get_component<render::SpriteRenderer>(
            entity, render::sprite_renderer_type());
        return c && read_sprite(*c, property, value);
    }
    if (component == "NEngine.SpriteAnimator") {
        const auto* animator = world.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        return animator && read_animator(*animator, property, value);
    }
    if (component == "NEngine.MeshRenderer") {
        const auto* c = world.get_component<render::MeshRenderer>(
            entity, render::mesh_renderer_type());
        return c && read_mesh(*c, property, value);
    }
    return false;
}

bool write_player_render_property(core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, const core::PropertyValue& value) {
    if (component == "NEngine.Camera") {
        auto* c = world.get_component<render::Camera>(
            entity, render::camera_type());
        return c && write_camera(*c, property, value);
    }
    if (component == "NEngine.Light") {
        auto* c = world.get_component<render::Light>(
            entity, render::light_type());
        return c && write_light(*c, property, value);
    }
    if (component == "NEngine.SpriteRenderer") {
        auto* c = world.get_component<render::SpriteRenderer>(
            entity, render::sprite_renderer_type());
        return c && write_sprite(*c, property, value);
    }
    if (component == "NEngine.SpriteAnimator") {
        auto* animator = world.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        return animator && write_animator(*animator, property, value);
    }
    if (component == "NEngine.MeshRenderer") {
        auto* c = world.get_component<render::MeshRenderer>(
            entity, render::mesh_renderer_type());
        return c && write_mesh(*c, property, value);
    }
    return false;
}
} // namespace nengine::player
