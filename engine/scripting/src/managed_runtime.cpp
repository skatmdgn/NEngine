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

    if (abi_version != 2) {
        diagnostic_ =
            "managed bridge ABI mismatch: expected 2, got " +
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
        "managed gameplay runtime initialized; ABI v2 lifecycle ready";

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
    count_ = nullptr;
    host_.shutdown();
}

} // namespace nengine::scripting
