#pragma once
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include "nengine/core/scene.hpp"

namespace nengine::core {

enum class AsyncScenePoll { Idle, Pending, Applied, Failed };

// Stage file IO, parse, component restoration and hierarchy construction on
// a worker; only swap the fully validated world on the caller's thread.
// poll_and_apply() never waits for disk IO or component restoration.
class AsyncSceneLoader {
public:
    bool request(const std::filesystem::path& file,
                 const ComponentSerializationRegistry* codecs = nullptr);
    AsyncScenePoll poll_and_apply(World& destination,
                                  std::string* error = nullptr);
    bool busy() const;
private:
    struct Outcome {
        bool ok{false};
        World staged{};
        std::string error{};
    };
    mutable std::future<Outcome> task_{};
};

} // namespace nengine::core
