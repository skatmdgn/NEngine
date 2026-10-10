#include "nengine/scripting/script_system.hpp"

#include <cmath>
#include <string>
#include <unordered_set>
#include <utility>

#include "nengine/scripting/components.hpp"

namespace nengine::scripting {
namespace {

class RuntimeWorldBinding {
public:
    RuntimeWorldBinding(
        ManagedRuntime* runtime,
        core::World* world) noexcept
        : runtime_(runtime) {

        if (runtime_) {
            runtime_->bind_world(
                world);
        }
    }

    ~RuntimeWorldBinding() {
        if (runtime_) {
            runtime_->bind_world(
                nullptr);
        }
    }

    RuntimeWorldBinding(
        const RuntimeWorldBinding&) = delete;
    RuntimeWorldBinding& operator=(
        const RuntimeWorldBinding&) = delete;

private:
    ManagedRuntime* runtime_{nullptr};
};

} // namespace

ManagedScriptSystem::~ManagedScriptSystem() {
    clear();
}

void ManagedScriptSystem::bind(
    ManagedRuntime* runtime) noexcept {

    if (runtime_ == runtime) {
        return;
    }

    clear();
    runtime_ = runtime;
}

ManagedScriptUpdateStats
ManagedScriptSystem::update(
    core::World& world,
    float delta_seconds,
    std::string* error) {

    ManagedScriptUpdateStats stats;

    if (!runtime_ ||
        !runtime_->valid()) {

        if (error) {
            *error =
                "managed script system has no valid runtime";
        }

        return stats;
    }

    if (!std::isfinite(
            delta_seconds) ||
        delta_seconds < 0.0f) {

        if (error) {
            *error =
                "managed script delta must be finite and non-negative";
        }

        return stats;
    }

    RuntimeWorldBinding world_binding{
        runtime_,
        &world
    };

    if (!runtime_->advance_frame(
            delta_seconds)) {

        ++stats.unresolved;

        if (error) {
            *error =
                runtime_->diagnostic();
        }

        return stats;
    }

    const auto push_native_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            const auto* transform =
                world.transform(
                    entity);

            if (!transform ||
                !runtime_->set_transform(
                    handle,
                    *transform) ||
                !runtime_->set_game_object(
                    handle,
                    entity,
                    world.name(entity),
                    world.active(entity))) {

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }

                return false;
            }

            return true;
        };

    const auto pull_managed_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            auto* transform =
                world.transform(
                    entity);

            if (!transform ||
                !runtime_->get_transform(
                    handle,
                    *transform)) {

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }

                return false;
            }

            core::Entity
                managed_entity =
                    core::Entity::invalid();

            std::string managed_name;
            bool managed_active = true;

            if (!runtime_->get_game_object(
                    handle,
                    managed_entity,
                    managed_name,
                    managed_active)) {

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }

                return false;
            }

            if (managed_entity != entity ||
                managed_name.empty() ||
                !world.set_name(
                    entity,
                    std::move(
                        managed_name)) ||
                !world.set_active(
                    entity,
                    managed_active)) {

                if (error) {
                    *error =
                        "managed GameObject state is invalid for native World entity";
                }

                return false;
            }

            return true;
        };

    std::unordered_set<
        core::Entity::value_type>
        active;

    for (const auto entity :
         world.entities()) {

        const auto* behaviour =
            world.get_component<
                ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

        if (!behaviour ||
            !behaviour->enabled ||
            behaviour->type_name.empty() ||
            !world.active(
                entity)) {
            continue;
        }

        active.insert(
            entity.value);

        auto existing =
            instances_.find(
                entity.value);

        if (existing !=
                instances_.end() &&
            existing->second.type_name !=
                behaviour->type_name) {

            if (runtime_->destroy(
                    existing->second.handle)) {
                ++stats.destroyed;
            } else {
                ++stats.unresolved;
            }

            instances_.erase(
                existing);

            existing =
                instances_.end();
        }

        if (existing ==
            instances_.end()) {

            const auto handle =
                runtime_->create_behaviour(
                    behaviour->type_name);

            if (!handle.valid()) {
                ++stats.unresolved;

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }

                continue;
            }

            Instance instance;
            instance.handle =
                handle;
            instance.type_name =
                behaviour->type_name;

            const auto [created, inserted] =
                instances_.emplace(
                    entity.value,
                    std::move(
                        instance));

            (void)inserted;

            ++stats.created;

            if (!push_native_state(
                    entity,
                    created->second.handle)) {

                ++stats.unresolved;

                runtime_->destroy(
                    created->second.handle);

                instances_.erase(
                    created);

                continue;
            }

            if (!runtime_->start(
                    created->second.handle)) {

                ++stats.unresolved;

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }

                runtime_->destroy(
                    created->second.handle);

                instances_.erase(
                    created);

                continue;
            }

            ++stats.started;

            if (!pull_managed_state(
                    entity,
                    created->second.handle)) {

                ++stats.unresolved;
            }

            existing =
                instances_.find(
                    entity.value);

            if (!world.active(
                    entity)) {
                continue;
            }
        }

        if (existing !=
                instances_.end()) {

            if (!push_native_state(
                    entity,
                    existing->second.handle)) {

                ++stats.unresolved;
                continue;
            }

            if (runtime_->update(
                    existing->second.handle,
                    delta_seconds)) {

                ++stats.updated;

                if (!pull_managed_state(
                        entity,
                        existing->second.handle)) {

                    ++stats.unresolved;
                }
            } else {
                ++stats.unresolved;

                if (error) {
                    *error =
                        runtime_->diagnostic();
                }
            }
        }
    }

    for (auto it =
             instances_.begin();
         it != instances_.end();) {

        if (active.contains(
                it->first)) {
            ++it;
            continue;
        }

        if (runtime_->destroy(
                it->second.handle)) {

            ++stats.destroyed;
        } else {
            ++stats.unresolved;
        }

        it =
            instances_.erase(
                it);
    }

    return stats;
}

void ManagedScriptSystem::clear() noexcept {
    if (runtime_ &&
        runtime_->valid()) {

        for (const auto& [_, instance] :
             instances_) {

            runtime_->destroy(
                instance.handle);
        }
    }

    instances_.clear();
}

} // namespace nengine::scripting
