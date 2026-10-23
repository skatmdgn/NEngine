#include "nengine/core/async_scene.hpp"
#include <chrono>
#include <exception>
#include <future>
#include <optional>
#include <utility>

namespace nengine::core {

bool AsyncSceneLoader::request(const std::filesystem::path& file,
                               const ComponentSerializationRegistry* codecs) {
    if (task_.valid() || file.empty()) return false;
    // The registry is copied before spawning, avoiding dangling references
    // and allowing asynchronous restoration of supported components.
    std::optional<ComponentSerializationRegistry> local;
    if(codecs) local = *codecs;
    task_ = std::async(std::launch::async,
        [file, codecs_copy=std::move(local)]() -> Outcome {
            Outcome result;
            try {
                SceneData data;
                if(!SceneSerializer::load_file(file,data,&result.error))return result;
                if(!SceneSerializer::instantiate(data,result.staged,&result.error,
                      codecs_copy ? &*codecs_copy : nullptr))return result;
                result.ok=true;
            } catch (const std::exception& exc) {
                result.error=exc.what();
            } catch (...) {
                result.error="unknown async scene loader failure";
            }
            return result;
        });
    return true;
}
bool AsyncSceneLoader::busy() const {
    return task_.valid();
}
AsyncScenePoll AsyncSceneLoader::poll_and_apply(
    World& destination,std::string* error) {
    if(!task_.valid())return AsyncScenePoll::Idle;
    if(task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
        return AsyncScenePoll::Pending;
    try {
        auto result=task_.get();
        if(!result.ok){
            if(error)*error=result.error;
            return AsyncScenePoll::Failed;
        }
        destination=std::move(result.staged);
        if(error)error->clear();
        return AsyncScenePoll::Applied;
    } catch (const std::exception& exc) {
        if(error)*error=exc.what();
    } catch (...) {
        if(error)*error="async scene worker failed";
    }
    return AsyncScenePoll::Failed;
}
} // namespace nengine::core
