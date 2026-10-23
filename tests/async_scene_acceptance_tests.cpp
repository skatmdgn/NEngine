#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "nengine/core/async_scene.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/render/particle.hpp"

namespace {
int failures=0;
void check(bool yes,const char* why){
    if(!yes){++failures;std::cerr<<"FAIL: "<<why<<"\n";}
}
nengine::core::AsyncScenePoll collect(
    nengine::core::AsyncSceneLoader& loader,
    nengine::core::World& world,
    std::string& error) {
    using namespace nengine::core;
    for(int attempt=0;attempt<5000;++attempt) {
        auto result=loader.poll_and_apply(world,&error);
        if(result!=AsyncScenePoll::Pending)return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return AsyncScenePoll::Pending;
}
}
int main(){
    using namespace nengine;
    using namespace nengine::core;
    std::error_code ec;
    const auto root=std::filesystem::temp_directory_path()/"nengine-async-scenes-20261023";
    std::filesystem::remove_all(root,ec);
    std::filesystem::create_directories(root,ec);
    const auto valid=root/"valid.nscene",bad=root/"bad.nscene";
    World authoring;
    for(int i=0;i<512;++i)authoring.create("Async Object "+std::to_string(i));
    auto scene=SceneSerializer::capture(authoring,"Async");
    std::string error;
    check(SceneSerializer::save_file(scene,valid,&error),"SCN-001: save large source");
    {std::ofstream file(bad,std::ios::binary);
     file<<"NENGINE_SCENE 3\nNAME \"Corrupt\"\nOBJECTS 12\nOBJECT";}
    World runtime;
    const auto keep=runtime.create("Persist");
    runtime.transform(keep)->local_position={4,5,6};
    AsyncSceneLoader loader;
    check(loader.request(valid),"SCN-001: launches async request without synchronous load");
    check(!loader.request(valid),"SCN-001: concurrent scene requests rejected");
    const auto loaded=collect(loader,runtime,error);
    check(loaded==AsyncScenePoll::Applied &&runtime.size()==512 &&
          runtime.name(runtime.entities()[0])=="Async Object 0" &&
          loader.poll_and_apply(runtime)==AsyncScenePoll::Idle,
          "SCN-001: completed staging is atomically installed and task cleaned");
    check(loader.request(bad),"SCN-002: accepts malformed file for async validation");
    const auto failed=collect(loader,runtime,error);
    check(failed==AsyncScenePoll::Failed &&runtime.size()==512 &&
          runtime.name(runtime.entities()[0])=="Async Object 0"&&!error.empty(),
          "SCN-002: parser failure preserves existing World and emits diagnostic");
    const auto missing=root/"not-here.nscene";
    check(loader.request(missing),"SCN-002: missing input dispatched");
    check(collect(loader,runtime,error)==AsyncScenePoll::Failed &&runtime.size()==512,
          "SCN-002: missing disk Scene is transactional");
    check(loader.request(valid)&&collect(loader,runtime,error)==AsyncScenePoll::Applied,
          "SCN-001: retry after error restores new Scene");

    core::ComponentSerializationRegistry codecs;
    render::register_particle_serializers(codecs);
    World effects;
    const auto particle=effects.create("ParticleOwner");
    auto* emitter=effects.add_component<render::ParticleEmitter>(
        particle,render::particle_emitter_type());
    emitter->initial_burst=7;
    auto scene2=SceneSerializer::capture(effects,"Effects",&codecs);
    const auto effect_file=root/"effects.nscene";
    check(SceneSerializer::save_file(scene2,effect_file,&error),
          "SCN-001: componentized Scene saved");
    check(loader.request(effect_file,&codecs) &&
          collect(loader,runtime,error)==AsyncScenePoll::Applied &&
          runtime.size()==1,"SCN-001: componentized scene instantiated off-thread");
    if(runtime.size()==1){
        const auto* saved=runtime.get_component<render::ParticleEmitter>(
            runtime.entities()[0],render::particle_emitter_type());
        check(saved&&saved->initial_burst==7,
              "SCN-001: copied codec lifetime safe for background restore");
    }
    std::filesystem::remove_all(root,ec);
    if(!failures)std::cout<<"NEngineAsyncSceneAcceptanceTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
