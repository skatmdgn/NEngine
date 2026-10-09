#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "nengine/core/entity.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/scripting/dotnet_host.hpp"

namespace nengine::core {
class World;
}

namespace nengine::input {
class InputState;
}

namespace nengine::scripting {

struct ManagedBehaviourHandle {
    std::int64_t value{0};

    bool valid() const noexcept {
        return value > 0;
    }

    friend bool operator==(
        ManagedBehaviourHandle,
        ManagedBehaviourHandle) = default;
};

class ManagedRuntime {
public:
    ManagedRuntime() = default;
    ~ManagedRuntime();

    ManagedRuntime(
        const ManagedRuntime&) = delete;
    ManagedRuntime& operator=(
        const ManagedRuntime&) = delete;

    ManagedRuntime(
        ManagedRuntime&&) noexcept;
    ManagedRuntime& operator=(
        ManagedRuntime&&) noexcept;

    bool initialize(
        const std::filesystem::path& hostfxr_path,
        const std::filesystem::path& runtime_config_path,
        const std::filesystem::path& assembly_path,
        std::string_view assembly_name);

    ManagedBehaviourHandle create_behaviour(
        std::string_view type_name);

    bool start(
        ManagedBehaviourHandle handle);

    bool update(
        ManagedBehaviourHandle handle,
        float delta_seconds);

    bool destroy(
        ManagedBehaviourHandle handle);

    bool set_transform(
        ManagedBehaviourHandle handle,
        const core::Transform& transform);

    bool get_transform(
        ManagedBehaviourHandle handle,
        core::Transform& transform);

    bool set_game_object(
        ManagedBehaviourHandle handle,
        core::Entity entity,
        std::string_view name,
        bool active);

    bool get_game_object(
        ManagedBehaviourHandle handle,
        core::Entity& entity,
        std::string& name,
        bool& active);

    bool reload_gameplay(
        const std::filesystem::path& assembly_path,
        std::string_view assembly_name);

    void bind_world(
        core::World* world) noexcept;

    void bind_input(
        const input::InputState* input_state) noexcept;

    bool unload_gameplay();

    bool gameplay_loaded() const noexcept {
        return is_gameplay_loaded_ &&
            is_gameplay_loaded_() > 0;
    }

    bool previous_load_context_alive() const noexcept {
        return previous_context_alive_ &&
            previous_context_alive_() > 0;
    }

    int instance_count() const;

    bool valid() const noexcept {
        return host_.ready() &&
            create_ != nullptr &&
            start_ != nullptr &&
            update_ != nullptr &&
            destroy_ != nullptr &&
            set_transform_ != nullptr &&
            get_transform_ != nullptr &&
            set_game_object_ != nullptr &&
            get_game_object_ != nullptr &&
            copy_game_object_name_ != nullptr &&
            configure_world_callbacks_ != nullptr &&
            configure_input_callbacks_ != nullptr &&
            world_context_ != nullptr &&
            load_gameplay_ != nullptr &&
            unload_gameplay_ != nullptr &&
            is_gameplay_loaded_ != nullptr &&
            previous_context_alive_ != nullptr &&
            count_ != nullptr &&
            gameplay_loaded();
    }

    const std::string& diagnostic()
        const noexcept {
        return diagnostic_;
    }

    void shutdown() noexcept;

private:
    using CreateFn =
        std::int64_t (*)(
            const char*);

    using InvokeFn =
        int (*)(
            std::int64_t);

    using UpdateFn =
        int (*)(
            std::int64_t,
            float);

    struct NativeTransformState {
        float px{0.0f};
        float py{0.0f};
        float pz{0.0f};
        float rx{0.0f};
        float ry{0.0f};
        float rz{0.0f};
        float rw{1.0f};
        float sx{1.0f};
        float sy{1.0f};
        float sz{1.0f};
    };

    using TransformFn =
        int (*)(
            std::int64_t,
            NativeTransformState*);

    struct NativeGameObjectState {
        std::uint64_t entity_id{0};
        std::int32_t active{1};
        std::int32_t name_bytes{1};
    };

    using SetGameObjectFn =
        int (*)(
            std::int64_t,
            std::uint64_t,
            int,
            const char*);

    using GetGameObjectFn =
        int (*)(
            std::int64_t,
            NativeGameObjectState*);

    using CopyGameObjectNameFn =
        int (*)(
            std::int64_t,
            char*,
            int);

    struct NativeWorldContext {
        core::World* world{nullptr};
        const input::InputState* input{nullptr};
    };

    using WorldCreateFn =
        std::uint64_t (*)(
            void*,
            const char*);

    using WorldDestroyFn =
        int (*)(
            void*,
            std::uint64_t);

    using WorldFindFn =
        std::uint64_t (*)(
            void*,
            const char*);

    using WorldIsAliveFn =
        int (*)(
            void*,
            std::uint64_t);

    using WorldNameFn =
        int (*)(
            void*,
            std::uint64_t,
            char*,
            int);

    using WorldSetNameFn =
        int (*)(
            void*,
            std::uint64_t,
            const char*);

    using WorldActiveFn =
        int (*)(
            void*,
            std::uint64_t);

    using WorldSetActiveFn =
        int (*)(
            void*,
            std::uint64_t,
            int);

    using WorldTransformFn =
        int (*)(
            void*,
            std::uint64_t,
            NativeTransformState*);

    using WorldParentFn =
        std::uint64_t (*)(
            void*,
            std::uint64_t);

    using WorldSetParentFn =
        int (*)(
            void*,
            std::uint64_t,
            std::uint64_t);

    using WorldChildCountFn =
        int (*)(
            void*,
            std::uint64_t);

    using WorldChildAtFn =
        std::uint64_t (*)(
            void*,
            std::uint64_t,
            int);

    using WorldHasComponentFn =
        int (*)(
            void*,
            std::uint64_t,
            const char*);

    struct NativeWorldCallbacks {
        void* context{nullptr};
        WorldCreateFn create{nullptr};
        WorldDestroyFn destroy{nullptr};
        WorldFindFn find{nullptr};
        WorldIsAliveFn is_alive{nullptr};
        WorldNameFn copy_name_utf8{nullptr};
        WorldSetNameFn set_name_utf8{nullptr};
        WorldActiveFn get_active{nullptr};
        WorldSetActiveFn set_active{nullptr};
        WorldTransformFn get_transform{nullptr};
        WorldTransformFn set_transform{nullptr};
        WorldParentFn get_parent{nullptr};
        WorldSetParentFn set_parent{nullptr};
        WorldChildCountFn get_child_count{nullptr};
        WorldChildAtFn get_child_at{nullptr};
        WorldHasComponentFn has_component{nullptr};
    };

    using ConfigureWorldCallbacksFn =
        int (*)(
            const NativeWorldCallbacks*);

    static std::uint64_t callback_create(
        void* context,
        const char* name);

    static int callback_destroy(
        void* context,
        std::uint64_t entity_id);

    static std::uint64_t callback_find(
        void* context,
        const char* name);

    static int callback_is_alive(
        void* context,
        std::uint64_t entity_id);

    static int callback_copy_name_utf8(
        void* context,
        std::uint64_t entity_id,
        char* buffer,
        int capacity);

    static int callback_set_name_utf8(
        void* context,
        std::uint64_t entity_id,
        const char* name);

    static int callback_get_active(
        void* context,
        std::uint64_t entity_id);

    static int callback_set_active(
        void* context,
        std::uint64_t entity_id,
        int active);

    static int callback_get_transform(
        void* context,
        std::uint64_t entity_id,
        NativeTransformState* state);

    static int callback_set_transform(
        void* context,
        std::uint64_t entity_id,
        NativeTransformState* state);

    static std::uint64_t callback_get_parent(
        void* context,
        std::uint64_t entity_id);

    static int callback_set_parent(
        void* context,
        std::uint64_t child_id,
        std::uint64_t parent_id);

    static int callback_get_child_count(
        void* context,
        std::uint64_t entity_id);

    static std::uint64_t callback_get_child_at(
        void* context,
        std::uint64_t entity_id,
        int index);

    static int callback_has_component(
        void* context,
        std::uint64_t entity_id,
        const char* type_name);

    using InputKeyFn =
        int (*)(
            void*,
            std::uint32_t);

    struct NativePointerState {
        float x{0.0f};
        float y{0.0f};
        float delta_x{0.0f};
        float delta_y{0.0f};
        float wheel_y{0.0f};
    };

    using InputPointerFn =
        int (*)(
            void*,
            NativePointerState*);

    struct NativeInputCallbacks {
        void* context{nullptr};
        InputKeyFn held{nullptr};
        InputKeyFn pressed{nullptr};
        InputKeyFn released{nullptr};
        InputPointerFn pointer{nullptr};
    };

    using ConfigureInputCallbacksFn =
        int (*)(
            const NativeInputCallbacks*);

    static int callback_input_held(
        void* context,
        std::uint32_t key);

    static int callback_input_pressed(
        void* context,
        std::uint32_t key);

    static int callback_input_released(
        void* context,
        std::uint32_t key);

    static int callback_input_pointer(
        void* context,
        NativePointerState* state);

    using LoadGameplayFn =
        int (*)(
            const char*,
            const char*);

    using SimpleFn =
        int (*)();

    using CountFn =
        int (*)();

    DotnetHost host_{};
    CreateFn create_{nullptr};
    InvokeFn start_{nullptr};
    UpdateFn update_{nullptr};
    InvokeFn destroy_{nullptr};
    TransformFn set_transform_{nullptr};
    TransformFn get_transform_{nullptr};
    SetGameObjectFn set_game_object_{nullptr};
    GetGameObjectFn get_game_object_{nullptr};
    CopyGameObjectNameFn copy_game_object_name_{nullptr};
    ConfigureWorldCallbacksFn configure_world_callbacks_{nullptr};
    ConfigureInputCallbacksFn configure_input_callbacks_{nullptr};
    std::unique_ptr<NativeWorldContext> world_context_{};
    LoadGameplayFn load_gameplay_{nullptr};
    SimpleFn unload_gameplay_{nullptr};
    SimpleFn is_gameplay_loaded_{nullptr};
    SimpleFn previous_context_alive_{nullptr};
    CountFn count_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::scripting
