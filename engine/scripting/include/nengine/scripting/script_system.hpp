#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "nengine/core/world.hpp"
#include "nengine/scripting/managed_runtime.hpp"

namespace nengine::scripting {

struct ManagedScriptUpdateStats {
    std::size_t created{0};
    std::size_t awoken{0};
    std::size_t enabled{0};
    std::size_t started{0};
    std::size_t updated{0};
    std::size_t late_updated{0};
    std::size_t disabled{0};
    std::size_t destroyed{0};
    std::size_t unresolved{0};
};

class ManagedScriptSystem {
public:
    ManagedScriptSystem() = default;
    ~ManagedScriptSystem();

    ManagedScriptSystem(
        const ManagedScriptSystem&) = delete;
    ManagedScriptSystem& operator=(
        const ManagedScriptSystem&) = delete;

    void bind(
        ManagedRuntime* runtime) noexcept;

    ManagedScriptUpdateStats update(
        core::World& world,
        float delta_seconds,
        std::string* error = nullptr);

    void clear(
        core::World* world = nullptr) noexcept;

    std::size_t instance_count()
        const noexcept {
        return instances_.size();
    }

private:
    struct Instance {
        ManagedBehaviourHandle handle{};
        std::string type_name{};
        bool active{false};
        bool started{false};
    };

    ManagedRuntime* runtime_{nullptr};

    std::unordered_map<
        core::Entity::value_type,
        Instance>
        instances_{};
};

} // namespace nengine::scripting
