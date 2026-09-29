#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/transform.hpp"

namespace nengine::core {

class World {
public:
    struct ObjectView {
        Entity entity;
        std::string_view name;
        bool active;
        const Transform& transform;
    };

    World() = default;

    Entity create(std::string name = "GameObject");
    bool destroy(Entity entity);
    bool is_alive(Entity entity) const noexcept;

    std::size_t size() const noexcept { return alive_count_; }

    std::string_view name(Entity entity) const;
    bool set_name(Entity entity, std::string name);

    bool active(Entity entity) const;
    bool set_active(Entity entity, bool value);

    Transform* transform(Entity entity);
    const Transform* transform(Entity entity) const;

    bool set_parent(Entity child, Entity parent);
    std::vector<Entity> children(Entity parent) const;

    std::optional<ObjectView> view(Entity entity) const;
    std::vector<Entity> entities() const;

    void clear();

private:
    struct Slot {
        std::uint32_t generation{0};
        bool alive{false};
        bool active{true};
        std::string name{};
        Transform transform{};
    };

    bool index_matches(Entity entity) const noexcept;
    bool would_create_cycle(Entity child, Entity parent) const noexcept;

    std::vector<Slot> slots_{};
    std::vector<std::uint32_t> free_indices_{};
    std::size_t alive_count_{0};
};

} // namespace nengine::core
