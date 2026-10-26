#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/property_value.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/scripting/dotnet_host.hpp"

namespace nengine::core {
class World;
}

namespace nengine::input {
class InputState;
class ActionMap;
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

    bool awake(
        ManagedBehaviourHandle handle);

    bool on_enable(
        ManagedBehaviourHandle handle);

    bool on_disable(
        ManagedBehaviourHandle handle);

    bool start(
        ManagedBehaviourHandle handle);

    bool update(
        ManagedBehaviourHandle handle,
        float delta_seconds);

    bool fixed_update(
        ManagedBehaviourHandle handle,
        float fixed_delta_seconds);

    bool late_update(
        ManagedBehaviourHandle handle);

    bool physics_event(
        ManagedBehaviourHandle handle,
        core::Entity other,
        int phase,
        bool is_trigger,
        bool is_2d,
        core::Vec3 normal,
        float penetration);

    bool advance_frame(
        float delta_seconds);

    bool reset_time();

    bool set_behaviour_enabled(
        ManagedBehaviourHandle handle,
        bool enabled);

    bool get_behaviour_enabled(
        ManagedBehaviourHandle handle,
        bool& enabled);

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

    using PropertyReadFn =
        bool (*)(
            void*,
            const core::World&,
            core::Entity,
            std::string_view,
            std::string_view,
            core::PropertyValue&);

    using PropertyWriteFn =
        bool (*)(
            void*,
            core::World&,
            core::Entity,
            std::string_view,
            std::string_view,
            const core::PropertyValue&);

    using PhysicsRaycastQueryFn =
        bool (*)(
            void*,
            const core::World&,
            bool,
            core::Vec3,
            core::Vec3,
            float,
            bool,
            std::uint32_t,
            core::Entity&,
            core::Vec3&,
            core::Vec3&,
            float&,
            bool&);

    using PhysicsBoxCastQueryFn =
        bool (*)(
            void*,
            const core::World&,
            bool,
            core::Vec3,
            core::Vec3,
            core::Vec3,
            float,
            bool,
            std::uint32_t,
            core::Entity&,
            core::Vec3&,
            core::Vec3&,
            float&,
            bool&);

    using PhysicsOverlapQueryFn =
        std::size_t (*)(
            void*,
            const core::World&,
            bool,
            core::Vec3,
            core::Vec3,
            bool,
            std::uint32_t,
            std::uint64_t*,
            std::size_t);

    void bind_property_access(
        void* context,
        PropertyReadFn read,
        PropertyWriteFn write) noexcept;

    void bind_physics_queries(
        void* context,
        PhysicsRaycastQueryFn raycast,
        PhysicsOverlapQueryFn overlap,
        PhysicsBoxCastQueryFn box_cast) noexcept;

    using ManagedLogFn = void (*)(void*, int, std::string_view, std::string_view, int);
    void bind_logging(void* context, ManagedLogFn log) noexcept;
    void bind_world(
        core::World* world) noexcept;

    std::vector<core::Entity>
    pending_world_destroys() const;

    bool flush_world_destroys();

    void bind_input(
        const input::InputState* input_state) noexcept;

    void bind_input_actions(
        const input::ActionMap* action_map) noexcept;

    // Domain-neutral callback boundary: Scripting never depends on NEngineUI.
    // Query op: 0=visible, 1=interactable, 2=clicked in current runtime frame.
    using UiFindFn = std::uint64_t (*)(void*, std::string_view);
    using UiQueryFn = int (*)(void*, std::uint64_t, int);
    using UiWriteFn = int (*)(void*, std::uint64_t, int, int);
    using UiTextFn = std::optional<std::string> (*)(void*, std::uint64_t);
    using UiSetTextFn = bool (*)(void*, std::uint64_t, std::string_view);
    using UiImageFn = std::optional<std::string> (*)(void*, std::uint64_t);
    using UiSetImageFn = bool (*)(void*, std::uint64_t, std::string_view);
    void bind_ui(void* context, UiFindFn find, UiQueryFn query,
                 UiWriteFn write, UiTextFn text,
                 UiSetTextFn set_text,
                 UiImageFn image = nullptr,
                 UiSetImageFn set_image = nullptr) noexcept;

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
            awake_ != nullptr &&
            on_enable_ != nullptr &&
            on_disable_ != nullptr &&
            start_ != nullptr &&
            update_ != nullptr &&
            fixed_update_ != nullptr &&
            late_update_ != nullptr &&
            physics_event_ != nullptr &&
            advance_frame_ != nullptr &&
            reset_time_ != nullptr &&
            set_behaviour_enabled_ != nullptr &&
            get_behaviour_enabled_ != nullptr &&
            destroy_ != nullptr &&
            set_transform_ != nullptr &&
            get_transform_ != nullptr &&
            set_game_object_ != nullptr &&
            get_game_object_ != nullptr &&
            copy_game_object_name_ != nullptr &&
            configure_world_callbacks_ != nullptr &&
            configure_input_callbacks_ != nullptr &&
            configure_ui_callbacks_ != nullptr &&
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

    using PhysicsEventFn =
        int (*)(
            std::int64_t,
            std::uint64_t,
            int,
            int,
            int,
            float,
            float,
            float,
            float);

    using FrameFn =
        int (*)(
            float);

    using SetEnabledFn =
        int (*)(
            std::int64_t,
            int);

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
        const input::ActionMap* action_map{nullptr};
        void* property_context{nullptr};
        PropertyReadFn property_read{nullptr};
        PropertyWriteFn property_write{nullptr};
        void* physics_context{nullptr};
        PhysicsRaycastQueryFn physics_raycast{nullptr};
        PhysicsOverlapQueryFn physics_overlap{nullptr};
        PhysicsBoxCastQueryFn physics_box_cast{nullptr};
        void* log_context{nullptr};
        ManagedLogFn log_fn{nullptr};
        void* ui_context{nullptr};
        UiFindFn ui_find{nullptr};
        UiQueryFn ui_query{nullptr};
        UiWriteFn ui_write{nullptr};
        UiTextFn ui_text{nullptr};
        UiSetTextFn ui_set_text{nullptr};
        UiImageFn ui_image{nullptr};
        UiSetImageFn ui_set_image{nullptr};
        std::vector<core::Entity> pending_destroy{};
    };

    struct NativePropertyValue {
        std::int32_t kind{0};
        std::int32_t boolean_value{0};
        std::int64_t integer_value{0};
        std::uint64_t unsigned_value{0};
        double number_value{0.0};
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
        float w{0.0f};
        char* text_buffer{nullptr};
        std::int32_t text_capacity{0};
        std::int32_t text_length{0};
    };

    struct NativeRaycastState {
        float ox{0.0f};
        float oy{0.0f};
        float oz{0.0f};
        float dx{0.0f};
        float dy{0.0f};
        float dz{0.0f};
        float max_distance{0.0f};
        std::int32_t include_triggers{1};
        std::uint32_t layer_mask{0xffffffffu};
        std::uint64_t hit_entity{
            core::Entity::invalid_value};
        float px{0.0f};
        float py{0.0f};
        float pz{0.0f};
        float nx{0.0f};
        float ny{0.0f};
        float nz{0.0f};
        float distance{0.0f};
        std::int32_t is_trigger{0};
    };

    struct NativeBoxCastState {
        float ox{0.0f};
        float oy{0.0f};
        float oz{0.0f};
        float sx{0.0f};
        float sy{0.0f};
        float sz{0.0f};
        float dx{0.0f};
        float dy{0.0f};
        float dz{0.0f};
        float max_distance{0.0f};
        std::int32_t include_triggers{1};
        std::uint32_t layer_mask{0xffffffffu};
        std::uint64_t hit_entity{
            core::Entity::invalid_value};
        float px{0.0f};
        float py{0.0f};
        float pz{0.0f};
        float nx{0.0f};
        float ny{0.0f};
        float nz{0.0f};
        float distance{0.0f};
        std::int32_t is_trigger{0};
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

    using WorldPropertyFn =
        int (*)(
            void*,
            std::uint64_t,
            const char*,
            const char*,
            NativePropertyValue*);

    using WorldPhysicsRaycastFn =
        int (*)(
            void*,
            int,
            NativeRaycastState*);

    using WorldPhysicsOverlapFn =
        int (*)(
            void*,
            int,
            float,
            float,
            float,
            float,
            float,
            float,
            int,
            std::uint32_t,
            std::uint64_t*,
            int);

    using WorldPhysicsBoxCastFn =
        int (*)(
            void*,
            int,
            NativeBoxCastState*);

    using WorldLogFn = int (*)(void*, int, const char*, const char*, int);
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
        WorldPropertyFn get_property{nullptr};
        WorldPropertyFn set_property{nullptr};
        WorldPhysicsRaycastFn physics_raycast{nullptr};
        WorldPhysicsOverlapFn physics_overlap{nullptr};
        WorldPhysicsBoxCastFn physics_box_cast{nullptr};
        WorldLogFn log{nullptr};
    };

    using ConfigureWorldCallbacksFn =
        int (*)(
            const NativeWorldCallbacks*);

    static int callback_log(void*, int, const char*, const char*, int);
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

    static int callback_get_property(
        void* context,
        std::uint64_t entity_id,
        const char* type_name,
        const char* property_name,
        NativePropertyValue* value);

    static int callback_set_property(
        void* context,
        std::uint64_t entity_id,
        const char* type_name,
        const char* property_name,
        NativePropertyValue* value);

    static int callback_physics_raycast(
        void* context,
        int is_2d,
        NativeRaycastState* state);

    static int callback_physics_overlap(
        void* context,
        int is_2d,
        float center_x,
        float center_y,
        float center_z,
        float size_x,
        float size_y,
        float size_z,
        int include_triggers,
        std::uint32_t layer_mask,
        std::uint64_t* output,
        int capacity);

    static int callback_physics_box_cast(
        void* context,
        int is_2d,
        NativeBoxCastState* state);

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

    // 0=held, 1=pressed, 2=released; nullptr map => unavailable.
    using InputActionFn = int (*)(void*, const char*, std::uint32_t);

    struct NativeInputCallbacks {
        void* context{nullptr};
        InputKeyFn held{nullptr};
        InputKeyFn pressed{nullptr};
        InputKeyFn released{nullptr};
        InputPointerFn pointer{nullptr};
        InputActionFn action{nullptr};
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

    static int callback_input_action(
        void* context, const char* name, std::uint32_t mode);

    using UiFindCallbackFn = std::uint64_t (*)(void*, const char*);
    using UiQueryCallbackFn = int (*)(void*, std::uint64_t, int);
    using UiWriteCallbackFn = int (*)(void*, std::uint64_t, int, int);
    using UiCopyTextCallbackFn = int (*)(void*, std::uint64_t, char*, int);
    using UiSetTextCallbackFn = int (*)(void*, std::uint64_t, const char*);
    using UiCopyImageCallbackFn = int (*)(void*, std::uint64_t, char*, int);
    using UiSetImageCallbackFn = int (*)(void*, std::uint64_t, const char*);
    struct NativeUiCallbacks {
        void* context{nullptr};
        UiFindCallbackFn find{nullptr};
        UiQueryCallbackFn query{nullptr};
        UiWriteCallbackFn write{nullptr};
        UiCopyTextCallbackFn copy_text{nullptr};
        UiSetTextCallbackFn set_text{nullptr};
        UiCopyImageCallbackFn copy_image{nullptr};
        UiSetImageCallbackFn set_image{nullptr};
    };
    using ConfigureUiCallbacksFn = int (*)(const NativeUiCallbacks*);
    static std::uint64_t callback_ui_find(void*, const char*);
    static int callback_ui_query(void*, std::uint64_t, int);
    static int callback_ui_write(void*, std::uint64_t, int, int);
    static int callback_ui_copy_text(void*, std::uint64_t, char*, int);
    static int callback_ui_set_text(void*, std::uint64_t, const char*);
    static int callback_ui_copy_image(void*, std::uint64_t, char*, int);
    static int callback_ui_set_image(void*, std::uint64_t, const char*);

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
    InvokeFn awake_{nullptr};
    InvokeFn on_enable_{nullptr};
    InvokeFn on_disable_{nullptr};
    InvokeFn start_{nullptr};
    UpdateFn update_{nullptr};
    UpdateFn fixed_update_{nullptr};
    InvokeFn late_update_{nullptr};
    PhysicsEventFn physics_event_{nullptr};
    FrameFn advance_frame_{nullptr};
    SimpleFn reset_time_{nullptr};
    SetEnabledFn set_behaviour_enabled_{nullptr};
    InvokeFn get_behaviour_enabled_{nullptr};
    InvokeFn destroy_{nullptr};
    TransformFn set_transform_{nullptr};
    TransformFn get_transform_{nullptr};
    SetGameObjectFn set_game_object_{nullptr};
    GetGameObjectFn get_game_object_{nullptr};
    CopyGameObjectNameFn copy_game_object_name_{nullptr};
    ConfigureWorldCallbacksFn configure_world_callbacks_{nullptr};
    ConfigureInputCallbacksFn configure_input_callbacks_{nullptr};
    ConfigureUiCallbacksFn configure_ui_callbacks_{nullptr};
    std::unique_ptr<NativeWorldContext> world_context_{};
    LoadGameplayFn load_gameplay_{nullptr};
    SimpleFn unload_gameplay_{nullptr};
    SimpleFn is_gameplay_loaded_{nullptr};
    SimpleFn previous_context_alive_{nullptr};
    CountFn count_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::scripting
