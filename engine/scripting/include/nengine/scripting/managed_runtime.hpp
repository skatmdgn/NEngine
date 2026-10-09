#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "nengine/scripting/dotnet_host.hpp"

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

    int instance_count() const;

    bool valid() const noexcept {
        return host_.ready() &&
            create_ != nullptr &&
            start_ != nullptr &&
            update_ != nullptr &&
            destroy_ != nullptr &&
            count_ != nullptr;
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

    using CountFn =
        int (*)();

    DotnetHost host_{};
    CreateFn create_{nullptr};
    InvokeFn start_{nullptr};
    UpdateFn update_{nullptr};
    InvokeFn destroy_{nullptr};
    CountFn count_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::scripting
