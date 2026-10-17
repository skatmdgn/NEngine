#include "managed_collider_properties.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <variant>

#include "nengine/physics/components.hpp"

namespace nengine::player {
namespace {
constexpr double max_extent = 1.0e6;

bool finite(const core::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool float_field(const core::PropertyValue& value, float& result,
                 double min, double max, bool exclusive_min = false) {
    const auto* n = std::get_if<double>(&value);
    if (!n || !std::isfinite(*n) || *n > max ||
        (exclusive_min ? *n <= min : *n < min)) return false;
    const float cast = static_cast<float>(*n);
    if (!std::isfinite(cast) ||
        (exclusive_min && cast <= static_cast<float>(min))) return false;
    result = cast;
    return true;
}

bool vector_field(const core::PropertyValue& value, core::Vec3& result,
                  bool positive_xyz, bool two_d) {
    const auto* v = std::get_if<core::Vec3>(&value);
    if (!v || !finite(*v) ||
        std::abs(v->x) > max_extent || std::abs(v->y) > max_extent ||
        std::abs(v->z) > max_extent) return false;
    if (positive_xyz && (v->x <= 0.f || v->y <= 0.f ||
        (!two_d && v->z <= 0.f))) return false;
    result = *v;
    if (two_d) result.z = 0.f;
    return true;
}

bool common_name(std::string_view property) {
    return property == "Enabled" || property == "Is Trigger" ||
           property == "Layer" || property == "Collision Mask" ||
           property == "Friction" || property == "Restitution" ||
           property == "Center";
}

template <typename T>
bool read_common(const T& c, std::string_view property,
                 core::PropertyValue& result) {
    if (property == "Enabled") result = c.enabled;
    else if (property == "Is Trigger") result = c.is_trigger;
    else if (property == "Layer") result = static_cast<std::int64_t>(c.layer);
    else if (property == "Collision Mask")
        result = static_cast<std::int64_t>(c.collision_mask);
    else if (property == "Friction") result = static_cast<double>(c.friction);
    else if (property == "Restitution")
        result = static_cast<double>(c.restitution);
    else if (property == "Center") result = c.center;
    else return false;
    return true;
}

template <typename T>
bool write_common(T& c, std::string_view property,
                  const core::PropertyValue& value, bool two_d) {
    if (property == "Enabled" || property == "Is Trigger") {
        const auto* b = std::get_if<bool>(&value);
        if (!b) return false;
        if (property == "Enabled") c.enabled = *b;
        else c.is_trigger = *b;
        return true;
    }
    if (property == "Layer" || property == "Collision Mask") {
        const auto* n = std::get_if<std::int64_t>(&value);
        if (!n || *n < 0 ||
            (property == "Layer" ? *n > 31 :
              *n > static_cast<std::int64_t>(0xffffffffu))) return false;
        if (property == "Layer") c.layer = static_cast<std::uint32_t>(*n);
        else c.collision_mask = static_cast<std::uint32_t>(*n);
        return true;
    }
    if (property == "Friction" || property == "Restitution") {
        float next{};
        if (!float_field(value, next, 0.0, 1.0)) return false;
        if (property == "Friction") c.friction = next;
        else c.restitution = next;
        return true;
    }
    if (property == "Center") {
        core::Vec3 next{};
        if (!vector_field(value, next, false, two_d)) return false;
        c.center = next;
        return true;
    }
    return false;
}
} // namespace

bool read_player_collider_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.BoxCollider") {
        const auto* c = world.get_component<physics::BoxCollider>(entity, physics::box_collider_type());
        if (!c) return false;
        if (common_name(property)) return read_common(*c, property, value);
        if (property == "Size") { value = c->size; return true; }
        return false;
    }
    if (component == "NEngine.SphereCollider") {
        const auto* c = world.get_component<physics::SphereCollider>(entity, physics::sphere_collider_type());
        if (!c) return false;
        if (common_name(property)) return read_common(*c, property, value);
        if (property == "Radius") {
            value = static_cast<double>(c->radius);
            return true;
        }
        return false;
    }
    if (component == "NEngine.CapsuleCollider") {
        const auto* c = world.get_component<physics::CapsuleCollider>(entity, physics::capsule_collider_type());
        if (!c) return false;
        if (common_name(property)) return read_common(*c, property, value);
        if (property == "Radius") value = static_cast<double>(c->radius);
        else if (property == "Height") value = static_cast<double>(c->height);
        else if (property == "Direction")
            value = static_cast<std::int64_t>(c->direction);
        else return false;
        return true;
        return false;
    }
    if (component == "NEngine.BoxCollider2D") {
        const auto* c = world.get_component<physics::BoxCollider2D>(entity, physics::box_collider2d_type());
        if (!c) return false;
        if (common_name(property)) return read_common(*c, property, value);
        if (property == "Size") { value = c->size; return true; }
        return false;
    }
    if (component == "NEngine.CircleCollider2D") {
        const auto* c = world.get_component<physics::CircleCollider2D>(entity, physics::circle_collider2d_type());
        if (!c) return false;
        if (common_name(property)) return read_common(*c, property, value);
        if (property == "Radius") {
            value = static_cast<double>(c->radius);
            return true;
        }
        return false;
    }
    return false;
}

bool write_player_collider_property(core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, const core::PropertyValue& value) {
    if (component == "NEngine.BoxCollider") {
        auto* c = world.get_component<physics::BoxCollider>(entity, physics::box_collider_type());
        if (!c) return false;
        if (common_name(property)) return write_common(*c, property, value, false);
        if (property == "Size") {
            core::Vec3 next{};
            if (!vector_field(value, next, true, false)) return false;
            c->size = next;
            return true;
        }
        return false;
    }
    if (component == "NEngine.SphereCollider") {
        auto* c = world.get_component<physics::SphereCollider>(entity, physics::sphere_collider_type());
        if (!c) return false;
        if (common_name(property)) return write_common(*c, property, value, false);
        if (property == "Radius") {
            float next{};
            if (!float_field(value, next, 0.0, max_extent, true)) return false;
            c->radius = next;
            return true;
        }
        return false;
    }
    if (component == "NEngine.CapsuleCollider") {
        auto* c = world.get_component<physics::CapsuleCollider>(entity, physics::capsule_collider_type());
        if (!c) return false;
        if (common_name(property)) return write_common(*c, property, value, false);
        if (property == "Radius" || property == "Height") {
            float next{};
            if (!float_field(value, next, 0.0, max_extent, true)) return false;
            const float radius = property == "Radius" ? next : c->radius;
            const float height = property == "Height" ? next : c->height;
            if (height < 2.0f * radius) return false;
            if (property == "Radius") c->radius = next;
            else c->height = next;
            return true;
        }
        if (property == "Direction") {
            const auto* n = std::get_if<std::int64_t>(&value);
            if (!n || *n < 0 || *n > 2) return false;
            c->direction = static_cast<std::uint32_t>(*n);
            return true;
        }
        return false;
    }
    if (component == "NEngine.BoxCollider2D") {
        auto* c = world.get_component<physics::BoxCollider2D>(entity, physics::box_collider2d_type());
        if (!c) return false;
        if (common_name(property)) return write_common(*c, property, value, true);
        if (property == "Size") {
            core::Vec3 next{};
            if (!vector_field(value, next, true, true)) return false;
            c->size = next;
            return true;
        }
        return false;
    }
    if (component == "NEngine.CircleCollider2D") {
        auto* c = world.get_component<physics::CircleCollider2D>(entity, physics::circle_collider2d_type());
        if (!c) return false;
        if (common_name(property)) return write_common(*c, property, value, true);
        if (property == "Radius") {
            float next{};
            if (!float_field(value, next, 0.0, max_extent, true)) return false;
            c->radius = next;
            return true;
        }
        return false;
    }
    return false;
}
} // namespace nengine::player
