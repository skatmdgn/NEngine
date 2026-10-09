#include "nengine/scripting/managed_runtime.hpp"

#include <cmath>
#include <utility>

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

    const std::string bridge_type =
        "NEngine.Internal.NativeBridge, " +
        std::string{
            assembly_name};

    using AbiFn =
        int (*)();

    const auto abi =
        load_entry<AbiFn>(
            host_,
            assembly_path,
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

    if (abi_version != 3) {
        diagnostic_ =
            "managed bridge ABI mismatch: expected 3, got " +
            std::to_string(
                abi_version);
        shutdown();
        return false;
    }

    create_ =
        load_entry<CreateFn>(
            host_,
            assembly_path,
            bridge_type,
            "CreateBehaviour");

    start_ =
        load_entry<InvokeFn>(
            host_,
            assembly_path,
            bridge_type,
            "InvokeStart");

    update_ =
        load_entry<UpdateFn>(
            host_,
            assembly_path,
            bridge_type,
            "InvokeUpdate");

    destroy_ =
        load_entry<InvokeFn>(
            host_,
            assembly_path,
            bridge_type,
            "DestroyBehaviour");

    set_transform_ =
        load_entry<TransformFn>(
            host_,
            assembly_path,
            bridge_type,
            "SetTransformState");

    get_transform_ =
        load_entry<TransformFn>(
            host_,
            assembly_path,
            bridge_type,
            "GetTransformState");

    count_ =
        load_entry<CountFn>(
            host_,
            assembly_path,
            bridge_type,
            "GetInstanceCount");

    if (!valid()) {
        diagnostic_ =
            "managed bridge is missing one or more lifecycle entry points";
        shutdown();
        return false;
    }

    diagnostic_ =
        "managed gameplay runtime initialized; ABI v3 lifecycle and Transform sync ready";

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
    count_ = nullptr;
    host_.shutdown();
}

} // namespace nengine::scripting
