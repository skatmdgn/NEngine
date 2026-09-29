#include "nengine/core/world.hpp"

#include <algorithm>
#include <utility>

namespace nengine::core {

const ComponentTypeId World::transform_type = ComponentRegistry::stable_id("NEngine.Transform");

World::World() {
    component_pools_.emplace(transform_type, std::make_unique<ComponentPool<Transform>>());
}

World::World(const World& other) {
    copy_from(other);
}

World& World::operator=(const World& other) {
    if (this != &other) {
        copy_from(other);
    }
    return *this;
}

void World::copy_from(const World& other) {
    slots_ = other.slots_;
    free_indices_ = other.free_indices_;
    alive_count_ = other.alive_count_;
    component_pools_.clear();
    for (const auto& [type, pool] : other.component_pools_) {
        component_pools_.emplace(type, pool->clone());
    }
}

Entity World::create(std::string name) {
    std::uint32_t index = 0;
    if (!free_indices_.empty()) {
        index = free_indices_.back();
        free_indices_.pop_back();
    } else {
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    }

    auto& slot = slots_[index];
    slot.alive = true;
    slot.active = true;
    slot.name = name.empty() ? "GameObject" : std::move(name);

    auto* transforms = ensure_pool<Transform>(transform_type);
    transforms->erase(index);
    transforms->emplace(index);

    ++alive_count_;
    return Entity::make(index, slot.generation);
}

bool World::destroy(Entity entity) {
    if (!is_alive(entity)) {
        return false;
    }

    for (const auto candidate : entities()) {
        auto* candidate_transform = transform(candidate);
        if (candidate_transform && candidate_transform->parent == entity) {
            candidate_transform->parent = Entity::invalid();
        }
    }

    for (auto& [_, pool] : component_pools_) {
        pool->erase(entity.index());
    }

    auto& slot = slots_[entity.index()];
    slot.alive = false;
    slot.active = false;
    slot.name.clear();
    ++slot.generation;
    free_indices_.push_back(entity.index());
    --alive_count_;
    return true;
}

bool World::index_matches(Entity entity) const noexcept {
    return entity.valid() && entity.index() < slots_.size() &&
           slots_[entity.index()].generation == entity.generation();
}

bool World::is_alive(Entity entity) const noexcept {
    return index_matches(entity) && slots_[entity.index()].alive;
}

std::string_view World::name(Entity entity) const {
    return is_alive(entity) ? std::string_view{slots_[entity.index()].name} : std::string_view{};
}

bool World::set_name(Entity entity, std::string name) {
    if (!is_alive(entity) || name.empty()) {
        return false;
    }
    slots_[entity.index()].name = std::move(name);
    return true;
}

bool World::active(Entity entity) const {
    return is_alive(entity) && slots_[entity.index()].active;
}

bool World::set_active(Entity entity, bool value) {
    if (!is_alive(entity)) {
        return false;
    }
    slots_[entity.index()].active = value;
    return true;
}

Transform* World::transform(Entity entity) {
    return get_component<Transform>(entity, transform_type);
}

const Transform* World::transform(Entity entity) const {
    return get_component<Transform>(entity, transform_type);
}

bool World::would_create_cycle(Entity child, Entity parent) const noexcept {
    auto current = parent;
    while (is_alive(current)) {
        if (current == child) {
            return true;
        }
        const auto* current_transform = transform(current);
        if (!current_transform) {
            break;
        }
        current = current_transform->parent;
    }
    return false;
}

bool World::set_parent(Entity child, Entity parent) {
    if (!is_alive(child)) {
        return false;
    }
    if (parent.valid() && !is_alive(parent)) {
        return false;
    }
    if (child == parent || (parent.valid() && would_create_cycle(child, parent))) {
        return false;
    }
    transform(child)->parent = parent;
    return true;
}

std::vector<Entity> World::children(Entity parent) const {
    std::vector<Entity> result;
    for (const auto candidate : entities()) {
        const auto* candidate_transform = transform(candidate);
        if (candidate_transform && candidate_transform->parent == parent) {
            result.push_back(candidate);
        }
    }
    return result;
}

bool World::remove_component(Entity entity, ComponentTypeId type) {
    if (!is_alive(entity) || type == transform_type) {
        return false;
    }
    const auto it = component_pools_.find(type);
    if (it == component_pools_.end() || !it->second->contains(entity.index())) {
        return false;
    }
    it->second->erase(entity.index());
    return true;
}

bool World::has_component(Entity entity, ComponentTypeId type) const {
    if (!is_alive(entity)) {
        return false;
    }
    const auto it = component_pools_.find(type);
    return it != component_pools_.end() && it->second->contains(entity.index());
}

std::vector<ComponentTypeId> World::component_types(Entity entity) const {
    std::vector<ComponentTypeId> result;
    if (!is_alive(entity)) {
        return result;
    }
    for (const auto& [type, pool] : component_pools_) {
        if (pool->contains(entity.index())) {
            result.push_back(type);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<World::ObjectView> World::view(Entity entity) const {
    if (!is_alive(entity)) {
        return std::nullopt;
    }
    const auto* object_transform = transform(entity);
    if (!object_transform) {
        return std::nullopt;
    }
    const auto& slot = slots_[entity.index()];
    return ObjectView{entity, slot.name, slot.active, *object_transform};
}

std::vector<Entity> World::entities() const {
    std::vector<Entity> result;
    result.reserve(alive_count_);
    for (std::uint32_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) {
            result.push_back(Entity::make(i, slots_[i].generation));
        }
    }
    return result;
}

void World::clear() {
    slots_.clear();
    free_indices_.clear();
    alive_count_ = 0;
    component_pools_.clear();
    component_pools_.emplace(transform_type, std::make_unique<ComponentPool<Transform>>());
}

} // namespace nengine::core
