#include "nengine/core/world.hpp"

#include <stdexcept>
#include <utility>

namespace nengine::core {

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
    slot.transform = {};
    ++alive_count_;
    return Entity::make(index, slot.generation);
}

bool World::destroy(Entity entity) {
    if (!is_alive(entity)) {
        return false;
    }

    for (auto& slot : slots_) {
        if (slot.alive && slot.transform.parent == entity) {
            slot.transform.parent = Entity::invalid();
        }
    }

    auto& slot = slots_[entity.index()];
    slot.alive = false;
    slot.active = false;
    slot.name.clear();
    slot.transform = {};
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
    if (!is_alive(entity)) {
        return {};
    }
    return slots_[entity.index()].name;
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
    return is_alive(entity) ? &slots_[entity.index()].transform : nullptr;
}

const Transform* World::transform(Entity entity) const {
    return is_alive(entity) ? &slots_[entity.index()].transform : nullptr;
}

bool World::would_create_cycle(Entity child, Entity parent) const noexcept {
    auto current = parent;
    while (is_alive(current)) {
        if (current == child) {
            return true;
        }
        current = slots_[current.index()].transform.parent;
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
    slots_[child.index()].transform.parent = parent;
    return true;
}

std::vector<Entity> World::children(Entity parent) const {
    std::vector<Entity> result;
    for (std::uint32_t i = 0; i < slots_.size(); ++i) {
        const auto& slot = slots_[i];
        if (slot.alive && slot.transform.parent == parent) {
            result.push_back(Entity::make(i, slot.generation));
        }
    }
    return result;
}

std::optional<World::ObjectView> World::view(Entity entity) const {
    if (!is_alive(entity)) {
        return std::nullopt;
    }
    const auto& slot = slots_[entity.index()];
    return ObjectView{entity, slot.name, slot.active, slot.transform};
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
}

} // namespace nengine::core
