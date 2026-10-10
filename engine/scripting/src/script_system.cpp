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
ManagedScriptSystem::fixed_update(
    core::World& world,
    float fixed_delta_seconds,
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

    if (!std::isfinite(fixed_delta_seconds) ||
        fixed_delta_seconds < 0.0f) {
        if (error) {
            *error =
                "managed script fixed delta must be finite and non-negative";
        }
        return stats;
    }

    RuntimeWorldBinding world_binding{
        runtime_,
        &world
    };

    const auto runtime_error =
        [&]() {
            if (error) {
                *error = runtime_->diagnostic();
            }
        };

    const auto push_native_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            const auto* transform =
                world.transform(entity);

            if (!transform ||
                !runtime_->set_transform(handle, *transform) ||
                !runtime_->set_game_object(
                    handle,
                    entity,
                    world.name(entity),
                    world.active(entity))) {

                runtime_error();
                return false;
            }

            return true;
        };

    const auto pull_managed_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            auto* transform =
                world.transform(entity);

            if (!transform ||
                !runtime_->get_transform(handle, *transform)) {
                runtime_error();
                return false;
            }

            core::Entity managed_entity =
                core::Entity::invalid();
            std::string managed_name;
            bool managed_active = true;

            if (!runtime_->get_game_object(
                    handle,
                    managed_entity,
                    managed_name,
                    managed_active)) {
                runtime_error();
                return false;
            }

            if (managed_entity != entity ||
                managed_name.empty() ||
                !world.set_name(entity, std::move(managed_name)) ||
                !world.set_active(entity, managed_active)) {

                if (error) {
                    *error =
                        "managed GameObject state is invalid for native World entity";
                }
                return false;
            }

            return true;
        };

    const auto push_enabled =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            const auto* behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            if (!behaviour ||
                !runtime_->set_behaviour_enabled(
                    handle,
                    behaviour->enabled)) {
                runtime_error();
                return false;
            }

            return true;
        };

    const auto pull_enabled =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            auto* behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());
            bool managed_enabled = false;

            if (!behaviour ||
                !runtime_->get_behaviour_enabled(
                    handle,
                    managed_enabled)) {
                runtime_error();
                return false;
            }

            behaviour->enabled =
                managed_enabled;
            return true;
        };

    const auto pull_all =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {
            return pull_managed_state(entity, handle) &&
                   pull_enabled(entity, handle);
        };

    const auto invoke_disable =
        [&](core::Entity entity,
            Instance& instance) {

            if (!instance.active) {
                return true;
            }

            if (!runtime_->on_disable(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                return false;
            }

            ++stats.disabled;
            instance.active = false;

            if (world.is_alive(entity) &&
                !pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                return false;
            }

            return true;
        };

    std::unordered_set<
        core::Entity::value_type>
        present;

    for (const auto entity :
         world.entities()) {

        auto* behaviour =
            world.get_component<ScriptBehaviour>(
                entity,
                script_behaviour_type());

        if (!behaviour ||
            behaviour->type_name.empty()) {
            continue;
        }

        present.insert(entity.value);
        const std::string type_name =
            behaviour->type_name;

        auto existing =
            instances_.find(entity.value);

        if (existing != instances_.end() &&
            existing->second.type_name != type_name) {

            invoke_disable(entity, existing->second);

            if (runtime_->destroy(existing->second.handle)) {
                ++stats.destroyed;
            } else {
                ++stats.unresolved;
                runtime_error();
            }

            instances_.erase(existing);
            existing = instances_.end();
        }

        if (existing == instances_.end()) {
            const auto handle =
                runtime_->create_behaviour(type_name);

            if (!handle.valid()) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            Instance instance;
            instance.handle = handle;
            instance.type_name = type_name;

            const auto [created, inserted] =
                instances_.emplace(
                    entity.value,
                    std::move(instance));
            (void)inserted;
            ++stats.created;

            if (!push_native_state(entity, created->second.handle) ||
                !push_enabled(entity, created->second.handle) ||
                !runtime_->awake(created->second.handle)) {

                ++stats.unresolved;
                runtime_error();
                runtime_->destroy(created->second.handle);
                instances_.erase(created);
                continue;
            }

            ++stats.awoken;

            if (!pull_all(entity, created->second.handle)) {
                ++stats.unresolved;
                runtime_->destroy(created->second.handle);
                instances_.erase(created);
                continue;
            }

            existing = instances_.find(entity.value);
        }

        if (existing == instances_.end() ||
            !world.is_alive(entity)) {
            continue;
        }

        behaviour =
            world.get_component<ScriptBehaviour>(
                entity,
                script_behaviour_type());

        if (!behaviour) {
            continue;
        }

        auto& instance =
            existing->second;

        if (!runtime_->set_behaviour_enabled(
                instance.handle,
                behaviour->enabled)) {
            ++stats.unresolved;
            runtime_error();
            continue;
        }

        bool should_be_active =
            world.active(entity) &&
            behaviour->enabled;

        if (should_be_active &&
            !instance.active) {

            if (!push_native_state(entity, instance.handle) ||
                !runtime_->on_enable(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            ++stats.enabled;
            instance.active = true;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
                continue;
            }
        } else if (!should_be_active &&
                   instance.active) {
            invoke_disable(entity, instance);
            continue;
        }

        if (!instance.active) {
            continue;
        }

        if (!instance.started) {
            if (!push_native_state(entity, instance.handle) ||
                !push_enabled(entity, instance.handle) ||
                !runtime_->start(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            instance.started = true;
            ++stats.started;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
                continue;
            }
        }

        if (!push_native_state(entity, instance.handle) ||
            !push_enabled(entity, instance.handle)) {
            ++stats.unresolved;
            continue;
        }

        if (runtime_->fixed_update(
                instance.handle,
                fixed_delta_seconds)) {
            ++stats.fixed_updated;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
            }
        } else {
            ++stats.unresolved;
            runtime_error();
        }
    }

    const auto pending_destroy =
        runtime_->pending_world_destroys();

    for (const auto entity :
         pending_destroy) {

        auto existing =
            instances_.find(entity.value);

        if (existing == instances_.end()) {
            continue;
        }

        invoke_disable(entity, existing->second);

        if (runtime_->destroy(existing->second.handle)) {
            ++stats.destroyed;
        } else {
            ++stats.unresolved;
            runtime_error();
        }

        instances_.erase(existing);
    }

    if (!runtime_->flush_world_destroys()) {
        ++stats.unresolved;
        runtime_error();
    }

    for (auto it = instances_.begin();
         it != instances_.end();) {

        const core::Entity instance_entity{
            it->first};

        if (present.contains(it->first) &&
            world.is_alive(instance_entity)) {
            ++it;
            continue;
        }

        invoke_disable(instance_entity, it->second);

        if (runtime_->destroy(it->second.handle)) {
            ++stats.destroyed;
        } else {
            ++stats.unresolved;
            runtime_error();
        }

        it = instances_.erase(it);
    }

    return stats;
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

    if (!std::isfinite(delta_seconds) ||
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

    if (!runtime_->advance_frame(delta_seconds)) {
        ++stats.unresolved;
        if (error) {
            *error = runtime_->diagnostic();
        }
        return stats;
    }

    const auto runtime_error =
        [&]() {
            if (error) {
                *error = runtime_->diagnostic();
            }
        };

    const auto push_native_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            const auto* transform =
                world.transform(entity);

            if (!transform ||
                !runtime_->set_transform(handle, *transform) ||
                !runtime_->set_game_object(
                    handle,
                    entity,
                    world.name(entity),
                    world.active(entity))) {

                runtime_error();
                return false;
            }

            return true;
        };

    const auto pull_managed_state =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            auto* transform =
                world.transform(entity);

            if (!transform ||
                !runtime_->get_transform(handle, *transform)) {
                runtime_error();
                return false;
            }

            core::Entity managed_entity =
                core::Entity::invalid();
            std::string managed_name;
            bool managed_active = true;

            if (!runtime_->get_game_object(
                    handle,
                    managed_entity,
                    managed_name,
                    managed_active)) {
                runtime_error();
                return false;
            }

            if (managed_entity != entity ||
                managed_name.empty() ||
                !world.set_name(entity, std::move(managed_name)) ||
                !world.set_active(entity, managed_active)) {

                if (error) {
                    *error =
                        "managed GameObject state is invalid for native World entity";
                }
                return false;
            }

            return true;
        };

    const auto push_enabled =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            const auto* behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            if (!behaviour ||
                !runtime_->set_behaviour_enabled(
                    handle,
                    behaviour->enabled)) {
                runtime_error();
                return false;
            }

            return true;
        };

    const auto pull_enabled =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {

            auto* behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());
            bool managed_enabled = false;

            if (!behaviour ||
                !runtime_->get_behaviour_enabled(
                    handle,
                    managed_enabled)) {
                runtime_error();
                return false;
            }

            behaviour->enabled =
                managed_enabled;
            return true;
        };

    const auto pull_all =
        [&](core::Entity entity,
            ManagedBehaviourHandle handle) {
            return pull_managed_state(entity, handle) &&
                   pull_enabled(entity, handle);
        };

    const auto invoke_disable =
        [&](core::Entity entity,
            Instance& instance) {

            if (!instance.active) {
                return true;
            }

            if (!runtime_->on_disable(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                return false;
            }

            ++stats.disabled;
            instance.active = false;

            if (world.is_alive(entity) &&
                !pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                return false;
            }

            return true;
        };

    std::unordered_set<
        core::Entity::value_type>
        present;

    for (const auto entity :
         world.entities()) {

        auto* behaviour =
            world.get_component<ScriptBehaviour>(
                entity,
                script_behaviour_type());

        if (!behaviour ||
            behaviour->type_name.empty()) {
            continue;
        }

        present.insert(entity.value);
        const std::string type_name =
            behaviour->type_name;

        auto existing =
            instances_.find(entity.value);

        if (existing != instances_.end() &&
            existing->second.type_name != type_name) {

            invoke_disable(entity, existing->second);

            if (runtime_->destroy(existing->second.handle)) {
                ++stats.destroyed;
            } else {
                ++stats.unresolved;
                runtime_error();
            }

            instances_.erase(existing);
            existing = instances_.end();
        }

        if (existing == instances_.end()) {
            const auto handle =
                runtime_->create_behaviour(type_name);

            if (!handle.valid()) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            Instance instance;
            instance.handle = handle;
            instance.type_name = type_name;

            const auto [created, inserted] =
                instances_.emplace(
                    entity.value,
                    std::move(instance));
            (void)inserted;
            ++stats.created;

            if (!push_native_state(entity, created->second.handle) ||
                !push_enabled(entity, created->second.handle) ||
                !runtime_->awake(created->second.handle)) {

                ++stats.unresolved;
                runtime_error();
                runtime_->destroy(created->second.handle);
                instances_.erase(created);
                continue;
            }

            ++stats.awoken;

            if (!pull_all(entity, created->second.handle)) {
                ++stats.unresolved;
                runtime_->destroy(created->second.handle);
                instances_.erase(created);
                continue;
            }

            existing = instances_.find(entity.value);
        }

        if (existing == instances_.end() ||
            !world.is_alive(entity)) {
            continue;
        }

        behaviour =
            world.get_component<ScriptBehaviour>(
                entity,
                script_behaviour_type());

        if (!behaviour) {
            continue;
        }

        auto& instance =
            existing->second;

        if (!runtime_->set_behaviour_enabled(
                instance.handle,
                behaviour->enabled)) {
            ++stats.unresolved;
            runtime_error();
            continue;
        }

        bool should_be_active =
            world.active(entity) &&
            behaviour->enabled;

        if (should_be_active &&
            !instance.active) {

            if (!push_native_state(entity, instance.handle) ||
                !runtime_->on_enable(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            ++stats.enabled;
            instance.active = true;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
                continue;
            }
        } else if (!should_be_active &&
                   instance.active) {
            invoke_disable(entity, instance);
            continue;
        }

        if (!instance.active) {
            continue;
        }

        if (!instance.started) {
            if (!push_native_state(entity, instance.handle) ||
                !push_enabled(entity, instance.handle) ||
                !runtime_->start(instance.handle)) {
                ++stats.unresolved;
                runtime_error();
                continue;
            }

            instance.started = true;
            ++stats.started;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
                continue;
            }
        }

        if (!push_native_state(entity, instance.handle) ||
            !push_enabled(entity, instance.handle)) {
            ++stats.unresolved;
            continue;
        }

        if (runtime_->update(
                instance.handle,
                delta_seconds)) {
            ++stats.updated;

            if (!pull_all(entity, instance.handle)) {
                ++stats.unresolved;
                continue;
            }

            behaviour =
                world.get_component<ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

            should_be_active =
                behaviour &&
                behaviour->enabled &&
                world.active(entity);

            if (!should_be_active) {
                invoke_disable(entity, instance);
            }
        } else {
            ++stats.unresolved;
            runtime_error();
        }
    }

    const auto pending_before_late =
        runtime_->pending_world_destroys();

    std::unordered_set<
        core::Entity::value_type>
        pending_before_late_ids;

    for (const auto entity :
         pending_before_late) {
        pending_before_late_ids.insert(
            entity.value);
    }

    // Every active Update completes before any active LateUpdate begins.
    for (const auto entity :
         world.entities()) {

        if (pending_before_late_ids.contains(
                entity.value)) {
            continue;
        }

        auto existing =
            instances_.find(
                entity.value);

        if (existing ==
                instances_.end() ||
            !existing->second.active ||
            !existing->second.started) {
            continue;
        }

        auto* behaviour =
            world.get_component<
                ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

        if (!behaviour ||
            !behaviour->enabled ||
            !world.active(
                entity)) {
            continue;
        }

        if (!push_native_state(
                entity,
                existing->second.handle) ||
            !push_enabled(
                entity,
                existing->second.handle)) {

            ++stats.unresolved;
            continue;
        }

        if (!runtime_->late_update(
                existing->second.handle)) {

            ++stats.unresolved;
            runtime_error();
            continue;
        }

        ++stats.late_updated;

        if (!pull_all(
                entity,
                existing->second.handle)) {

            ++stats.unresolved;
            continue;
        }

        behaviour =
            world.get_component<
                ScriptBehaviour>(
                    entity,
                    script_behaviour_type());

        const bool still_active =
            behaviour &&
            behaviour->enabled &&
            world.active(
                entity);

        if (!still_active) {
            invoke_disable(
                entity,
                existing->second);
        }
    }

    const auto pending_destroy =
        runtime_->pending_world_destroys();

    for (const auto entity :
         pending_destroy) {

        auto existing =
            instances_.find(entity.value);

        if (existing == instances_.end()) {
            continue;
        }

        invoke_disable(entity, existing->second);

        if (runtime_->destroy(existing->second.handle)) {
            ++stats.destroyed;
        } else {
            ++stats.unresolved;
            runtime_error();
        }

        instances_.erase(existing);
    }

    if (!runtime_->flush_world_destroys()) {
        ++stats.unresolved;
        runtime_error();
    }

    for (auto it = instances_.begin();
         it != instances_.end();) {

        const core::Entity instance_entity{
            it->first};

        if (present.contains(it->first) &&
            world.is_alive(instance_entity)) {
            ++it;
            continue;
        }

        invoke_disable(instance_entity, it->second);

        if (runtime_->destroy(it->second.handle)) {
            ++stats.destroyed;
        } else {
            ++stats.unresolved;
            runtime_error();
        }

        it = instances_.erase(it);
    }

    return stats;
}

bool ManagedScriptSystem::dispatch_physics_event(
    core::World& world,
    core::Entity target,
    core::Entity other,
    int phase,
    bool is_trigger,
    bool is_2d,
    core::Vec3 normal,
    float penetration,
    std::string* error) {

    if (!runtime_ ||
        !runtime_->valid() ||
        !world.is_alive(target) ||
        !world.is_alive(other) ||
        phase < 0 ||
        phase > 2) {

        if (error) {
            *error =
                "managed physics event dispatch received invalid runtime or entities";
        }
        return false;
    }

    auto existing =
        instances_.find(
            target.value);

    if (existing == instances_.end() ||
        !existing->second.active) {
        return true;
    }

    RuntimeWorldBinding world_binding{
        runtime_,
        &world
    };

    auto* transform =
        world.transform(target);

    const auto* behaviour =
        world.get_component<ScriptBehaviour>(
            target,
            script_behaviour_type());

    if (!transform ||
        !behaviour) {
        if (error) {
            *error =
                "managed physics event target has no Transform or ScriptBehaviour";
        }
        return false;
    }

    auto& instance =
        existing->second;

    if (!runtime_->set_transform(
            instance.handle,
            *transform) ||
        !runtime_->set_game_object(
            instance.handle,
            target,
            world.name(target),
            world.active(target)) ||
        !runtime_->set_behaviour_enabled(
            instance.handle,
            behaviour->enabled) ||
        !runtime_->physics_event(
            instance.handle,
            other,
            phase,
            is_trigger,
            is_2d,
            normal,
            penetration)) {

        if (error) {
            *error =
                runtime_->diagnostic();
        }
        return false;
    }

    transform =
        world.transform(target);

    if (!transform ||
        !runtime_->get_transform(
            instance.handle,
            *transform)) {

        if (error) {
            *error =
                runtime_->diagnostic();
        }
        return false;
    }

    core::Entity managed_entity =
        core::Entity::invalid();

    std::string managed_name;
    bool managed_active = true;

    if (!runtime_->get_game_object(
            instance.handle,
            managed_entity,
            managed_name,
            managed_active) ||
        managed_entity != target ||
        managed_name.empty() ||
        !world.set_name(
            target,
            std::move(managed_name)) ||
        !world.set_active(
            target,
            managed_active)) {

        if (error) {
            *error =
                "managed physics callback returned invalid GameObject state";
        }
        return false;
    }

    bool managed_enabled = false;

    if (!runtime_->get_behaviour_enabled(
            instance.handle,
            managed_enabled)) {

        if (error) {
            *error =
                runtime_->diagnostic();
        }
        return false;
    }

    auto* mutable_behaviour =
        world.get_component<ScriptBehaviour>(
            target,
            script_behaviour_type());

    if (mutable_behaviour) {
        mutable_behaviour->enabled =
            managed_enabled;
    }

    return true;
}

void ManagedScriptSystem::clear(
    core::World* world) noexcept {

    if (runtime_ &&
        runtime_->valid()) {

        RuntimeWorldBinding world_binding{
            runtime_,
            world
        };

        for (auto& [_, instance] :
             instances_) {

            if (instance.active) {
                runtime_->on_disable(instance.handle);
                instance.active = false;
            }

            runtime_->destroy(instance.handle);
        }

        if (world) {
            runtime_->flush_world_destroys();
        }
    }

    instances_.clear();
}

} // namespace nengine::scripting
