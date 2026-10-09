#include "nengine/scripting/managed_runtime.hpp"

#include <cmath>
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
      start_(
          std::exchange(
              other.start_,
              nullptr)),
      update_(
          std::exchange(
              other.update_,
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

    start_ =
        std::exchange(
            other.start_,
            nullptr);

    update_ =
        std::exchange(
            other.update_,
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

    if (abi_version != 5) {
        diagnostic_ =
            "managed bridge ABI mismatch: expected 5, got " +
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
        !start_ ||
        !update_ ||
        !destroy_ ||
        !set_transform_ ||
        !get_transform_ ||
        !set_game_object_ ||
        !get_game_object_ ||
        !copy_game_object_name_ ||
        !load_gameplay_ ||
        !unload_gameplay_ ||
        !is_gameplay_loaded_ ||
        !previous_context_alive_ ||
        !count_) {

        diagnostic_ =
            "managed bridge is missing one or more ABI v5 entry points";
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
        "managed gameplay runtime initialized; ABI v5 collectible gameplay lifecycle Transform and GameObject sync ready";

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
    create_ = nullptr;
    start_ = nullptr;
    update_ = nullptr;
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
    load_gameplay_ = nullptr;
    unload_gameplay_ = nullptr;
    is_gameplay_loaded_ = nullptr;
    previous_context_alive_ = nullptr;
    count_ = nullptr;
    host_.shutdown();
}

} // namespace nengine::scripting
