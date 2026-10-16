#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_storage.hpp"
#include "nengine/core/entity.hpp"
#include "nengine/core/transform.hpp"

namespace nengine::core {

class World {
public:
    static const ComponentTypeId transform_type;

    struct ObjectView {
        Entity entity;
        std::string_view name;
        bool active;
        const Transform& transform;
    };

    World();
    World(const World& other);
    World& operator=(const World& other);
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

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

    template <typename T, typename... Args>
    T* add_component(Entity entity, ComponentTypeId type, Args&&... args) {
        if (!is_alive(entity) || type == ComponentRegistry::invalid_type || type == transform_type) {
            return nullptr;
        }
        auto* pool = ensure_pool<T>(type);
        return pool ? pool->emplace(entity.index(), std::forward<Args>(args)...) : nullptr;
    }

    template <typename T>
    T* get_component(Entity entity, ComponentTypeId type) {
        if (!is_alive(entity)) {
            return nullptr;
        }
        auto* pool = find_pool<T>(type);
        return pool ? pool->get(entity.index()) : nullptr;
    }

    template <typename T>
    const T* get_component(Entity entity, ComponentTypeId type) const {
        if (!is_alive(entity)) {
            return nullptr;
        }
        const auto* pool = find_pool<T>(type);
        return pool ? pool->get(entity.index()) : nullptr;
    }

    bool remove_component(Entity entity, ComponentTypeId type);
    bool has_component(Entity entity, ComponentTypeId type) const;
    std::vector<ComponentTypeId> component_types(Entity entity) const;

    std::optional<ObjectView> view(Entity entity) const;
    std::vector<Entity> entities() const;

    World clone() const { return World{*this}; }
    void clear();

private:
    struct Slot {
        std::uint32_t generation{0};
        bool alive{false};
        bool active{true};
        std::string name{};
    };

    template <typename T>
    ComponentPool<T>* ensure_pool(ComponentTypeId type) {
        auto it = component_pools_.find(type);
        if (it == component_pools_.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto* raw = pool.get();
            component_pools_.emplace(type, std::move(pool));
            return raw;
        }
        if (it->second->value_type() != std::type_index{typeid(T)}) {
            return nullptr;
        }
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    ComponentPool<T>* find_pool(ComponentTypeId type) {
        const auto it = component_pools_.find(type);
        if (it == component_pools_.end() || it->second->value_type() != std::type_index{typeid(T)}) {
            return nullptr;
        }
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* find_pool(ComponentTypeId type) const {
        const auto it = component_pools_.find(type);
        if (it == component_pools_.end() || it->second->value_type() != std::type_index{typeid(T)}) {
            return nullptr;
        }
        return static_cast<const ComponentPool<T>*>(it->second.get());
    }

    bool index_matches(Entity entity) const noexcept;
    bool would_create_cycle(Entity child, Entity parent) const noexcept;
    void copy_from(const World& other);

    std::vector<Slot> slots_{};
    std::vector<std::uint32_t> free_indices_{};
    std::size_t alive_count_{0};
    std::unordered_map<ComponentTypeId, std::unique_ptr<ComponentPoolBase>> component_pools_{};
};

} // namespace nengine::core
