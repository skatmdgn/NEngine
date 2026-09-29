#pragma once

#include <algorithm>
#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

class Selection {
public:
    void clear() noexcept {
        entities_.clear();
        active_ = core::Entity::invalid();
    }

    void set(core::Entity entity) {
        entities_.clear();
        if (entity.valid()) {
            entities_.push_back(entity);
        }
        active_ = entity;
    }

    void add(core::Entity entity) {
        if (!entity.valid()) return;
        if (std::find(entities_.begin(), entities_.end(), entity) == entities_.end()) {
            entities_.push_back(entity);
        }
        active_ = entity;
    }

    void remove(core::Entity entity) {
        entities_.erase(std::remove(entities_.begin(), entities_.end(), entity), entities_.end());
        if (active_ == entity) {
            active_ = entities_.empty() ? core::Entity::invalid() : entities_.back();
        }
    }

    void sanitize(const core::World& world) {
        entities_.erase(std::remove_if(entities_.begin(), entities_.end(), [&](core::Entity entity) {
            return !world.is_alive(entity);
        }), entities_.end());
        if (!world.is_alive(active_)) {
            active_ = entities_.empty() ? core::Entity::invalid() : entities_.back();
        }
    }

    core::Entity active() const noexcept { return active_; }
    const std::vector<core::Entity>& entities() const noexcept { return entities_; }
    bool empty() const noexcept { return entities_.empty(); }

private:
    std::vector<core::Entity> entities_{};
    core::Entity active_{core::Entity::invalid()};
};

} // namespace nengine::editor
