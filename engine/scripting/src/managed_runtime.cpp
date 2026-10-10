#include "nengine/scripting/managed_runtime.hpp"

#include "nengine/core/component_registry.hpp"
#include "nengine/core/world.hpp"
#include "nengine/input/input_state.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace nengine::scripting {
namespace {

template <typename Function>
Function load_entry(
    DotnetHost& host,
    const std::filesystem::path& assembly_path,
    std::string_view bridge_type,
    std::string_view method) {

    return reinterpret_cast<Function>(
        host.load_unmanaged_entry(
            assembly_path,
            bridge_type,
            method));
}

std::string path_utf8(
    const std::filesystem::path& path) {

    const auto encoded =
        path.u8string();

    return std::string{
        reinterpret_cast<const char*>(
            encoded.data()),
        encoded.size()};
}

} // namespace

std::uint64_t ManagedRuntime::callback_create(
    void* context,
    const char* name) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return core::Entity::invalid_value;
    }

    const auto entity =
        state->world->create(
            name
                ? std::string{name}
                : std::string{"GameObject"});

    return entity.value;
}

int ManagedRuntime::callback_destroy(
    void* context,
    std::uint64_t entity_id) {

    auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    const core::Entity entity{
        entity_id};

    if (!state->world->is_alive(
            entity)) {
        return -1;
    }

    for (const auto queued :
         state->pending_destroy) {

        if (queued == entity) {
            return 1;
        }
    }

    state->pending_destroy.push_back(
        entity);

    return 1;
}

std::uint64_t ManagedRuntime::callback_find(
    void* context,
    const char* name) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !name) {
        return core::Entity::invalid_value;
    }

    for (const auto entity :
         state->world->entities()) {

        if (state->world->name(
                entity) ==
            name) {
            return entity.value;
        }
    }

    return core::Entity::invalid_value;
}

int ManagedRuntime::callback_is_alive(
    void* context,
    std::uint64_t entity_id) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    return state->world->is_alive(
        core::Entity{entity_id})
        ? 1
        : 0;
}

int ManagedRuntime::callback_copy_name_utf8(
    void* context,
    std::uint64_t entity_id,
    char* buffer,
    int capacity) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    const core::Entity entity{
        entity_id};

    if (!state->world->is_alive(
            entity)) {
        return -1;
    }

    const auto name =
        state->world->name(entity);

    if (name.size() >=
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return -1;
    }

    const int required =
        static_cast<int>(
            name.size() + 1u);

    if (!buffer ||
        capacity < required) {
        return -required;
    }

    if (!name.empty()) {
        std::memcpy(
            buffer,
            name.data(),
            name.size());
    }

    buffer[name.size()] = '\0';
    return required;
}

int ManagedRuntime::callback_set_name_utf8(
    void* context,
    std::uint64_t entity_id,
    const char* name) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !name) {
        return -1;
    }

    return state->world->set_name(
        core::Entity{entity_id},
        std::string{name})
        ? 1
        : -1;
}

int ManagedRuntime::callback_get_active(
    void* context,
    std::uint64_t entity_id) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    const core::Entity entity{
        entity_id};

    if (!state->world->is_alive(
            entity)) {
        return -1;
    }

    return state->world->active(
        entity)
        ? 1
        : 0;
}

int ManagedRuntime::callback_set_active(
    void* context,
    std::uint64_t entity_id,
    int active) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    return state->world->set_active(
        core::Entity{entity_id},
        active != 0)
        ? 1
        : -1;
}

int ManagedRuntime::callback_get_transform(
    void* context,
    std::uint64_t entity_id,
    NativeTransformState* output) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !output) {
        return -1;
    }

    const auto* transform =
        state->world->transform(
            core::Entity{entity_id});

    if (!transform) {
        return -1;
    }

    output->px = transform->local_position.x;
    output->py = transform->local_position.y;
    output->pz = transform->local_position.z;
    output->rx = transform->local_rotation.x;
    output->ry = transform->local_rotation.y;
    output->rz = transform->local_rotation.z;
    output->rw = transform->local_rotation.w;
    output->sx = transform->local_scale.x;
    output->sy = transform->local_scale.y;
    output->sz = transform->local_scale.z;

    return 1;
}

int ManagedRuntime::callback_set_transform(
    void* context,
    std::uint64_t entity_id,
    NativeTransformState* input) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !input) {
        return -1;
    }

    auto* transform =
        state->world->transform(
            core::Entity{entity_id});

    if (!transform) {
        return -1;
    }

    const float values[] = {
        input->px, input->py, input->pz,
        input->rx, input->ry, input->rz, input->rw,
        input->sx, input->sy, input->sz
    };

    for (const float value : values) {
        if (!std::isfinite(value)) {
            return -1;
        }
    }

    transform->local_position = {
        input->px, input->py, input->pz};

    transform->local_rotation = {
        input->rx, input->ry, input->rz, input->rw};

    transform->local_scale = {
        input->sx, input->sy, input->sz};

    return 1;
}

std::uint64_t ManagedRuntime::callback_get_parent(
    void* context,
    std::uint64_t entity_id) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return core::Entity::invalid_value;
    }

    const auto* transform =
        state->world->transform(
            core::Entity{entity_id});

    return transform
        ? transform->parent.value
        : core::Entity::invalid_value;
}

int ManagedRuntime::callback_set_parent(
    void* context,
    std::uint64_t child_id,
    std::uint64_t parent_id) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world) {
        return -1;
    }

    return state->world->set_parent(
        core::Entity{child_id},
        core::Entity{parent_id})
        ? 1
        : -1;
}

int ManagedRuntime::callback_get_child_count(
    void* context,
    std::uint64_t entity_id) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !state->world->is_alive(
            core::Entity{entity_id})) {
        return -1;
    }

    const auto children =
        state->world->children(
            core::Entity{entity_id});

    if (children.size() >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return -1;
    }

    return static_cast<int>(
        children.size());
}

std::uint64_t ManagedRuntime::callback_get_child_at(
    void* context,
    std::uint64_t entity_id,
    int index) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        index < 0) {
        return core::Entity::invalid_value;
    }

    const auto children =
        state->world->children(
            core::Entity{entity_id});

    if (static_cast<std::size_t>(
            index) >=
        children.size()) {
        return core::Entity::invalid_value;
    }

    return children[
        static_cast<std::size_t>(
            index)].value;
}

int ManagedRuntime::callback_has_component(
    void* context,
    std::uint64_t entity_id,
    const char* type_name) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->world ||
        !type_name ||
        *type_name == '\0') {
        return -1;
    }

    const core::Entity entity{
        entity_id};

    if (!state->world->is_alive(
            entity)) {
        return -1;
    }

    const auto type =
        core::ComponentRegistry::stable_id(
            type_name);

    if (type ==
        core::World::transform_type) {
        return state->world->transform(
            entity)
            ? 1
            : 0;
    }

    return state->world->has_component(
        entity,
        type)
        ? 1
        : 0;
}

void ManagedRuntime::bind_world(
    core::World* world) noexcept {

    if (!world_context_) {
        world_context_ =
            std::make_unique<
                NativeWorldContext>();
    }

    if (world_context_->world !=
            world) {
        world_context_
            ->pending_destroy
            .clear();
    }

    world_context_->world =
        world;
}

std::vector<core::Entity>
ManagedRuntime::pending_world_destroys()
    const {

    if (!world_context_) {
        return {};
    }

    return world_context_
        ->pending_destroy;
}

bool ManagedRuntime::flush_world_destroys() {
    if (!world_context_ ||
        !world_context_->world) {

        if (world_context_) {
            world_context_
                ->pending_destroy
                .clear();
        }

        diagnostic_ =
            "managed runtime has no bound World for deferred destroy flush";
        return false;
    }

    auto pending =
        std::move(
            world_context_
                ->pending_destroy);

    world_context_
        ->pending_destroy
        .clear();

    bool success = true;

    for (const auto entity :
         pending) {

        if (!world_context_
                ->world
                ->is_alive(
                    entity)) {
            continue;
        }

        if (!world_context_
                ->world
                ->destroy(
                    entity)) {
            success = false;
        }
    }

    if (!success) {
        diagnostic_ =
            "one or more deferred managed GameObject destroys failed";
    }

    return success;
}

void ManagedRuntime::bind_input(
    const input::InputState*
        input_state) noexcept {

    if (!world_context_) {
        world_context_ =
            std::make_unique<
                NativeWorldContext>();
    }

    world_context_->input =
        input_state;
}

int ManagedRuntime::callback_input_held(
    void* context,
    std::uint32_t key) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->input ||
        key >=
            static_cast<std::uint32_t>(
                input::Key::Count)) {
        return -1;
    }

    return state->input->held(
        static_cast<input::Key>(
            key))
        ? 1
        : 0;
}

int ManagedRuntime::callback_input_pressed(
    void* context,
    std::uint32_t key) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->input ||
        key >=
            static_cast<std::uint32_t>(
                input::Key::Count)) {
        return -1;
    }

    return state->input->pressed(
        static_cast<input::Key>(
            key))
        ? 1
        : 0;
}

int ManagedRuntime::callback_input_released(
    void* context,
    std::uint32_t key) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->input ||
        key >=
            static_cast<std::uint32_t>(
                input::Key::Count)) {
        return -1;
    }

    return state->input->released(
        static_cast<input::Key>(
            key))
        ? 1
        : 0;
}

int ManagedRuntime::callback_input_pointer(
    void* context,
    NativePointerState* output) {

    const auto* state =
        static_cast<NativeWorldContext*>(
            context);

    if (!state ||
        !state->input ||
        !output) {
        return -1;
    }

    const auto& pointer =
        state->input->pointer();

    output->x = pointer.x;
    output->y = pointer.y;
    output->delta_x =
        pointer.delta_x;
    output->delta_y =
        pointer.delta_y;
    output->wheel_y =
        pointer.wheel_y;

    return 1;
}

ManagedRuntime::~ManagedRuntime() {
    shutdown();
}

ManagedRuntime::ManagedRuntime(
    ManagedRuntime&& other) noexcept
    : host_(
          std::move(
              other.host_)),
      create_(
          std::exchange(
              other.create_,
              nullptr)),
      awake_(
          std::exchange(
              other.awake_,
              nullptr)),
      on_enable_(
          std::exchange(
              other.on_enable_,
              nullptr)),
      on_disable_(
          std::exchange(
              other.on_disable_,
              nullptr)),
      start_(
          std::exchange(
              other.start_,
              nullptr)),
      update_(
          std::exchange(
              other.update_,
              nullptr)),
      advance_frame_(
          std::exchange(
              other.advance_frame_,
              nullptr)),
      reset_time_(
          std::exchange(
              other.reset_time_,
              nullptr)),
      set_behaviour_enabled_(
          std::exchange(
              other.set_behaviour_enabled_,
              nullptr)),
      get_behaviour_enabled_(
          std::exchange(
              other.get_behaviour_enabled_,
              nullptr)),
      destroy_(
          std::exchange(
              other.destroy_,
              nullptr)),
      set_transform_(
          std::exchange(
              other.set_transform_,
              nullptr)),
      get_transform_(
          std::exchange(
              other.get_transform_,
              nullptr)),
      set_game_object_(
          std::exchange(
              other.set_game_object_,
              nullptr)),
      get_game_object_(
          std::exchange(
              other.get_game_object_,
              nullptr)),
      copy_game_object_name_(
          std::exchange(
              other.copy_game_object_name_,
              nullptr)),
      configure_world_callbacks_(
          std::exchange(
              other.configure_world_callbacks_,
              nullptr)),
      configure_input_callbacks_(
          std::exchange(
              other.configure_input_callbacks_,
              nullptr)),
      world_context_(
          std::move(
              other.world_context_)),
      load_gameplay_(
          std::exchange(
              other.load_gameplay_,
              nullptr)),
      unload_gameplay_(
          std::exchange(
              other.unload_gameplay_,
              nullptr)),
      is_gameplay_loaded_(
          std::exchange(
              other.is_gameplay_loaded_,
              nullptr)),
      previous_context_alive_(
          std::exchange(
              other.previous_context_alive_,
              nullptr)),
      count_(
          std::exchange(
              other.count_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

ManagedRuntime&
ManagedRuntime::operator=(
    ManagedRuntime&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    shutdown();

    host_ =
        std::move(
            other.host_);

    create_ =
        std::exchange(
            other.create_,
            nullptr);

    awake_ =
        std::exchange(
            other.awake_,
            nullptr);

    on_enable_ =
        std::exchange(
            other.on_enable_,
            nullptr);

    on_disable_ =
        std::exchange(
            other.on_disable_,
            nullptr);

    start_ =
        std::exchange(
            other.start_,
            nullptr);

    update_ =
        std::exchange(
            other.update_,
            nullptr);

    advance_frame_ =
        std::exchange(
            other.advance_frame_,
            nullptr);

    reset_time_ =
        std::exchange(
            other.reset_time_,
            nullptr);

    set_behaviour_enabled_ =
        std::exchange(
            other.set_behaviour_enabled_,
            nullptr);

    get_behaviour_enabled_ =
        std::exchange(
            other.get_behaviour_enabled_,
            nullptr);

    destroy_ =
        std::exchange(
            other.destroy_,
            nullptr);

    set_transform_ =
        std::exchange(
            other.set_transform_,
            nullptr);

    get_transform_ =
        std::exchange(
            other.get_transform_,
            nullptr);

    set_game_object_ =
        std::exchange(
            other.set_game_object_,
            nullptr);

    get_game_object_ =
        std::exchange(
            other.get_game_object_,
            nullptr);

    copy_game_object_name_ =
        std::exchange(
            other.copy_game_object_name_,
            nullptr);

    configure_world_callbacks_ =
        std::exchange(
            other.configure_world_callbacks_,
            nullptr);

    configure_input_callbacks_ =
        std::exchange(
            other.configure_input_callbacks_,
            nullptr);

    world_context_ =
        std::move(
            other.world_context_);

    load_gameplay_ =
        std::exchange(
            other.load_gameplay_,
            nullptr);

    unload_gameplay_ =
        std::exchange(
            other.unload_gameplay_,
            nullptr);

    is_gameplay_loaded_ =
        std::exchange(
            other.is_gameplay_loaded_,
            nullptr);

    previous_context_alive_ =
        std::exchange(
            other.previous_context_alive_,
            nullptr);

    count_ =
        std::exchange(
            other.count_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool ManagedRuntime::initialize(
    const std::filesystem::path&
        hostfxr_path,
    const std::filesystem::path&
        runtime_config_path,
    const std::filesystem::path&
        assembly_path,
    std::string_view assembly_name) {

    shutdown();

    if (assembly_path.empty() ||
        assembly_name.empty()) {

        diagnostic_ =
            "managed runtime requires gameplay assembly path and name";

        return false;
    }

    if (!host_.initialize(
            hostfxr_path,
            runtime_config_path)) {

        diagnostic_ =
            host_.diagnostic();

        return false;
    }

    const auto bridge_assembly_path =
        assembly_path.parent_path() /
        "NEngine.Bridge.dll";

    std::error_code bridge_error;

    if (!std::filesystem::is_regular_file(
            bridge_assembly_path,
            bridge_error) ||
        bridge_error) {

        diagnostic_ =
            "managed bridge assembly is missing beside gameplay assembly: " +
            bridge_assembly_path.generic_string();

        shutdown();
        return false;
    }

    const std::string bridge_type =
        "NEngine.Internal.NativeBridge, NEngine.Bridge";

    using AbiFn =
        int (*)();

    const auto abi =
        load_entry<AbiFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "GetAbiVersion");

    if (!abi) {
        diagnostic_ =
            host_.diagnostic();
        shutdown();
        return false;
    }

    const int abi_version =
        abi();

    if (abi_version != 8) {
        diagnostic_ =
            "managed bridge ABI mismatch: expected 8, got " +
            std::to_string(
                abi_version);
        shutdown();
        return false;
    }

    create_ =
        load_entry<CreateFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "CreateBehaviour");

    awake_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "InvokeAwake");

    on_enable_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "InvokeEnable");

    on_disable_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "InvokeDisable");

    start_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "InvokeStart");

    update_ =
        load_entry<UpdateFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "InvokeUpdate");

    advance_frame_ =
        load_entry<FrameFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "AdvanceFrameClock");

    reset_time_ =
        load_entry<SimpleFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "ResetFrameClock");

    set_behaviour_enabled_ =
        load_entry<SetEnabledFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "SetBehaviourEnabled");

    get_behaviour_enabled_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "GetBehaviourEnabled");

    destroy_ =
        load_entry<InvokeFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "DestroyBehaviour");

    set_transform_ =
        load_entry<TransformFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "SetTransformState");

    get_transform_ =
        load_entry<TransformFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "GetTransformState");

    set_game_object_ =
        load_entry<SetGameObjectFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "SetGameObjectState");

    get_game_object_ =
        load_entry<GetGameObjectFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "GetGameObjectState");

    copy_game_object_name_ =
        load_entry<CopyGameObjectNameFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "CopyGameObjectNameUtf8");

    configure_world_callbacks_ =
        load_entry<ConfigureWorldCallbacksFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "ConfigureNativeWorldCallbacks");

    configure_input_callbacks_ =
        load_entry<ConfigureInputCallbacksFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "ConfigureNativeInputCallbacks");

    load_gameplay_ =
        load_entry<LoadGameplayFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "LoadGameplayAssembly");

    unload_gameplay_ =
        load_entry<SimpleFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "UnloadGameplayAssembly");

    is_gameplay_loaded_ =
        load_entry<SimpleFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "IsGameplayAssemblyLoaded");

    previous_context_alive_ =
        load_entry<SimpleFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "IsPreviousLoadContextAlive");

    count_ =
        load_entry<CountFn>(
            host_,
            bridge_assembly_path,
            bridge_type,
            "GetInstanceCount");

    if (!create_ ||
        !awake_ ||
        !on_enable_ ||
        !on_disable_ ||
        !start_ ||
        !update_ ||
        !advance_frame_ ||
        !reset_time_ ||
        !set_behaviour_enabled_ ||
        !get_behaviour_enabled_ ||
        !destroy_ ||
        !set_transform_ ||
        !get_transform_ ||
        !set_game_object_ ||
        !get_game_object_ ||
        !copy_game_object_name_ ||
        !configure_world_callbacks_ ||
        !configure_input_callbacks_ ||
        !load_gameplay_ ||
        !unload_gameplay_ ||
        !is_gameplay_loaded_ ||
        !previous_context_alive_ ||
        !count_) {

        diagnostic_ =
            "managed bridge is missing one or more ABI v8 entry points";
        shutdown();
        return false;
    }

    if (!world_context_) {
        world_context_ =
            std::make_unique<
                NativeWorldContext>();
    }

    NativeWorldCallbacks callbacks;
    callbacks.context =
        world_context_.get();
    callbacks.create =
        &ManagedRuntime::callback_create;
    callbacks.destroy =
        &ManagedRuntime::callback_destroy;
    callbacks.find =
        &ManagedRuntime::callback_find;
    callbacks.is_alive =
        &ManagedRuntime::callback_is_alive;
    callbacks.copy_name_utf8 =
        &ManagedRuntime::callback_copy_name_utf8;
    callbacks.set_name_utf8 =
        &ManagedRuntime::callback_set_name_utf8;
    callbacks.get_active =
        &ManagedRuntime::callback_get_active;
    callbacks.set_active =
        &ManagedRuntime::callback_set_active;
    callbacks.get_transform =
        &ManagedRuntime::callback_get_transform;
    callbacks.set_transform =
        &ManagedRuntime::callback_set_transform;
    callbacks.get_parent =
        &ManagedRuntime::callback_get_parent;
    callbacks.set_parent =
        &ManagedRuntime::callback_set_parent;
    callbacks.get_child_count =
        &ManagedRuntime::callback_get_child_count;
    callbacks.get_child_at =
        &ManagedRuntime::callback_get_child_at;
    callbacks.has_component =
        &ManagedRuntime::callback_has_component;

    if (configure_world_callbacks_(
            &callbacks) <= 0) {

        diagnostic_ =
            "managed Bridge rejected native World callback table";

        shutdown();
        return false;
    }

    NativeInputCallbacks
        input_callbacks;

    input_callbacks.context =
        world_context_.get();
    input_callbacks.held =
        &ManagedRuntime::callback_input_held;
    input_callbacks.pressed =
        &ManagedRuntime::callback_input_pressed;
    input_callbacks.released =
        &ManagedRuntime::callback_input_released;
    input_callbacks.pointer =
        &ManagedRuntime::callback_input_pointer;

    if (configure_input_callbacks_(
            &input_callbacks) <= 0) {

        diagnostic_ =
            "managed Bridge rejected native Input callback table";

        shutdown();
        return false;
    }

    const auto gameplay_path =
        path_utf8(
            std::filesystem::absolute(
                assembly_path));

    std::string terminated_name{
        assembly_name};

    const int load_result =
        load_gameplay_(
            gameplay_path.c_str(),
            terminated_name.c_str());

    if (load_result < 0 ||
        !gameplay_loaded()) {

        diagnostic_ =
            "managed bridge could not load collectible gameplay assembly";

        shutdown();
        return false;
    }

    diagnostic_ =
        "managed gameplay runtime initialized; ABI v8 activation lifecycle native World lifetime input and coroutine callbacks ready";

    return true;
}

ManagedBehaviourHandle
ManagedRuntime::create_behaviour(
    std::string_view type_name) {

    if (!valid() ||
        type_name.empty()) {

        diagnostic_ =
            "managed runtime is not ready or script type is empty";

        return {};
    }

    std::string terminated{
        type_name};

    const auto value =
        create_(
            terminated.c_str());

    if (value <= 0) {
        diagnostic_ =
            "managed Behaviour creation failed for type: " +
            terminated;
        return {};
    }

    return {
        value};
}

bool ManagedRuntime::awake(
    ManagedBehaviourHandle handle) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        awake_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour Awake invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::on_enable(
    ManagedBehaviourHandle handle) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        on_enable_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour OnEnable invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::on_disable(
    ManagedBehaviourHandle handle) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        on_disable_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour OnDisable invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::start(
    ManagedBehaviourHandle handle) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        start_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour Start invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::advance_frame(
    float delta_seconds) {

    if (!valid() ||
        !std::isfinite(
            delta_seconds) ||
        delta_seconds < 0.0f) {

        return false;
    }

    const int result =
        advance_frame_(
            delta_seconds);

    if (result < 0) {
        diagnostic_ =
            "managed Time frame advance failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::reset_time() {

    if (!valid()) {
        return false;
    }

    const int result =
        reset_time_();

    if (result < 0) {
        diagnostic_ =
            "managed Time reset failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::update(
    ManagedBehaviourHandle handle,
    float delta_seconds) {

    if (!valid() ||
        !handle.valid() ||
        !std::isfinite(
            delta_seconds) ||
        delta_seconds < 0.0f) {

        return false;
    }

    const int result =
        update_(
            handle.value,
            delta_seconds);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour Update invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::set_behaviour_enabled(
    ManagedBehaviourHandle handle,
    bool enabled) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        set_behaviour_enabled_(
            handle.value,
            enabled ? 1 : 0);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour enabled-state push failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::get_behaviour_enabled(
    ManagedBehaviourHandle handle,
    bool& enabled) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        get_behaviour_enabled_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour enabled-state pull failed";
        return false;
    }

    enabled =
        result != 0;

    return true;
}

bool ManagedRuntime::destroy(
    ManagedBehaviourHandle handle) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    const int result =
        destroy_(
            handle.value);

    if (result < 0) {
        diagnostic_ =
            "managed Behaviour OnDestroy invocation failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::set_transform(
    ManagedBehaviourHandle handle,
    const core::Transform& transform) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    NativeTransformState state;
    state.px = transform.local_position.x;
    state.py = transform.local_position.y;
    state.pz = transform.local_position.z;
    state.rx = transform.local_rotation.x;
    state.ry = transform.local_rotation.y;
    state.rz = transform.local_rotation.z;
    state.rw = transform.local_rotation.w;
    state.sx = transform.local_scale.x;
    state.sy = transform.local_scale.y;
    state.sz = transform.local_scale.z;

    const int result =
        set_transform_(
            handle.value,
            &state);

    if (result < 0) {
        diagnostic_ =
            "managed Transform push failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::get_transform(
    ManagedBehaviourHandle handle,
    core::Transform& transform) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    NativeTransformState state;

    const int result =
        get_transform_(
            handle.value,
            &state);

    if (result < 0) {
        diagnostic_ =
            "managed Transform pull failed";
        return false;
    }

    transform.local_position = {
        state.px,
        state.py,
        state.pz
    };

    transform.local_rotation = {
        state.rx,
        state.ry,
        state.rz,
        state.rw
    };

    transform.local_scale = {
        state.sx,
        state.sy,
        state.sz
    };

    return true;
}

bool ManagedRuntime::set_game_object(
    ManagedBehaviourHandle handle,
    core::Entity entity,
    std::string_view name,
    bool active) {

    if (!valid() ||
        !handle.valid() ||
        !entity.valid()) {
        return false;
    }

    std::string terminated{
        name};

    const int result =
        set_game_object_(
            handle.value,
            entity.value,
            active ? 1 : 0,
            terminated.c_str());

    if (result < 0) {
        diagnostic_ =
            "managed GameObject push failed";
        return false;
    }

    return true;
}

bool ManagedRuntime::get_game_object(
    ManagedBehaviourHandle handle,
    core::Entity& entity,
    std::string& name,
    bool& active) {

    if (!valid() ||
        !handle.valid()) {
        return false;
    }

    NativeGameObjectState state;

    const int state_result =
        get_game_object_(
            handle.value,
            &state);

    if (state_result < 0 ||
        state.name_bytes <= 0 ||
        state.name_bytes >
            1024 * 1024) {

        diagnostic_ =
            "managed GameObject state pull failed";
        return false;
    }

    std::vector<char> buffer(
        static_cast<std::size_t>(
            state.name_bytes),
        '\0');

    const int copied =
        copy_game_object_name_(
            handle.value,
            buffer.data(),
            state.name_bytes);

    if (copied !=
            state.name_bytes ||
        buffer.empty() ||
        buffer.back() != '\0') {

        diagnostic_ =
            "managed GameObject name pull failed";
        return false;
    }

    entity.value =
        state.entity_id;

    active =
        state.active != 0;

    name.assign(
        buffer.data(),
        buffer.size() - 1u);

    return true;
}

bool ManagedRuntime::unload_gameplay() {

    if (!host_.ready() ||
        !unload_gameplay_ ||
        !is_gameplay_loaded_) {
        return false;
    }

    if (count_ &&
        count_() != 0) {

        diagnostic_ =
            "cannot unload gameplay assembly while managed Behaviour instances are alive";
        return false;
    }

    if (!gameplay_loaded()) {
        return true;
    }

    const int result =
        unload_gameplay_();

    if (result < 0 ||
        gameplay_loaded()) {

        diagnostic_ =
            "collectible gameplay assembly unload request failed";
        return false;
    }

    diagnostic_ =
        previous_load_context_alive()
            ? "gameplay assembly unloaded; collectible context is awaiting GC"
            : "gameplay assembly and collectible context unloaded";

    return true;
}

bool ManagedRuntime::reload_gameplay(
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name) {

    if (!host_.ready() ||
        !load_gameplay_ ||
        !unload_gameplay_ ||
        assembly_path.empty() ||
        assembly_name.empty()) {

        diagnostic_ =
            "managed runtime bridge is not ready for gameplay reload";
        return false;
    }

    if (!unload_gameplay()) {
        return false;
    }

    const auto gameplay_path =
        path_utf8(
            std::filesystem::absolute(
                assembly_path));

    std::string terminated_name{
        assembly_name};

    const int result =
        load_gameplay_(
            gameplay_path.c_str(),
            terminated_name.c_str());

    if (result < 0 ||
        !gameplay_loaded()) {

        diagnostic_ =
            "collectible gameplay assembly reload failed";
        return false;
    }

    diagnostic_ =
        "collectible gameplay assembly reloaded";

    return true;
}

int ManagedRuntime::instance_count()
    const {

    return valid()
        ? count_()
        : 0;
}

void ManagedRuntime::shutdown() noexcept {
    if (configure_input_callbacks_) {
        configure_input_callbacks_(
            nullptr);
    }

    if (configure_world_callbacks_) {
        configure_world_callbacks_(
            nullptr);
    }

    if (world_context_) {
        world_context_->world =
            nullptr;
        world_context_->input =
            nullptr;
    }

    create_ = nullptr;
    awake_ = nullptr;
    on_enable_ = nullptr;
    on_disable_ = nullptr;
    start_ = nullptr;
    update_ = nullptr;
    advance_frame_ = nullptr;
    reset_time_ = nullptr;
    set_behaviour_enabled_ = nullptr;
    get_behaviour_enabled_ = nullptr;
    destroy_ = nullptr;
    set_transform_ = nullptr;
    get_transform_ = nullptr;
    set_game_object_ = nullptr;
    get_game_object_ = nullptr;
    if (unload_gameplay_ &&
        is_gameplay_loaded_ &&
        count_ &&
        count_() == 0 &&
        gameplay_loaded()) {

        unload_gameplay_();
    }

    copy_game_object_name_ = nullptr;
    configure_world_callbacks_ = nullptr;
    configure_input_callbacks_ = nullptr;
    world_context_.reset();
    load_gameplay_ = nullptr;
    unload_gameplay_ = nullptr;
    is_gameplay_loaded_ = nullptr;
    previous_context_alive_ = nullptr;
    count_ = nullptr;
    host_.shutdown();
}

} // namespace nengine::scripting
