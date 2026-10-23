// Round 2 acceptance: CORE-009, CORE-010, CORE-011.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include "nengine/editor/play_session.hpp"
#include "nengine/core/scene.hpp"

namespace {
int failures=0;
void check(bool b,const char* what) { if(!b){++failures;std::cerr<<"FAIL: "<<what<<"\n";}}
std::string contents(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary);
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
void play_isolation() {
    using namespace nengine;
    core::World editor;
    const auto original=editor.create("Authoring");
    editor.transform(original)->local_position={1,2,3};
    const auto health=core::ComponentRegistry::stable_id("Tests.RuntimeOnly");
    editor.add_component<int>(original,health,42);
    editor::PlaySession session;
    for(int repeat=0;repeat<5;++repeat) {
        check(session.play(editor),"CORE-009: enters Play from editor world");
        auto* runtime=session.runtime_world();
        check(runtime!=nullptr,"CORE-009: runtime clone exists");
        if(!runtime)break;
        runtime->set_name(original,"ChangedOnlyInPlay");
        runtime->transform(original)->local_position={99,98,97};
        runtime->get_component<int>(original,health)[0]=7;
        runtime->destroy(original);
        runtime->create("RuntimeNew");
        check(session.pause() && session.step() && session.stop(),
              "CORE-009: Pause/Step/Stop roundtrip");
        check(session.runtime_world()==nullptr && editor.size()==1 &&
              editor.name(original)=="Authoring" &&
              editor.transform(original)->local_position==core::Vec3{1,2,3} &&
              editor.get_component<int>(original,health) &&
              *editor.get_component<int>(original,health)==42,
              "CORE-009: runtime changes never leak to editing World");
    }
}
void scene_switch() {
    using namespace nengine::core;
    World destination;
    const auto obsolete=destination.create("Old");
    World a; a.create("LevelA");
    World b; b.create("LevelB"); b.create("SecondObject");
    auto data_a=SceneSerializer::capture(a,"LevelA");
    auto data_b=SceneSerializer::capture(b,"LevelB");
    std::string error;
    check(SceneSerializer::instantiate(data_a,destination,&error) &&
          destination.size()==1 && destination.name(destination.entities().front())=="LevelA" &&
          !destination.is_alive(obsolete),"CORE-010: LevelA replaces old World");
    const auto level_a=destination.entities().front();
    auto bad=data_b;
    bad.objects[1].parent_local_id=123456;
    check(!SceneSerializer::instantiate(bad,destination,&error) &&
          destination.size()==1 && destination.is_alive(level_a) &&
          destination.name(level_a)=="LevelA" && !error.empty(),
          "CORE-010: invalid LevelB preserves LevelA with diagnostic");
    check(SceneSerializer::instantiate(data_b,destination,&error) &&
          destination.size()==2 && !destination.is_alive(level_a),
          "CORE-010: corrected LevelB replaces LevelA");
    check(SceneSerializer::instantiate(data_a,destination,&error) &&
          destination.size()==1 && destination.name(destination.entities().front())=="LevelA",
          "CORE-010: multiple A/B/A scene switches");
}
void atomic_saves() {
    using namespace nengine::core;
    std::error_code ec;
    auto root=std::filesystem::temp_directory_path()/"nengine-acceptance-core-20261023";
    std::filesystem::remove_all(root,ec);
    std::filesystem::create_directories(root,ec);
    const auto destination=root/"level.nscene";
    {std::ofstream f(destination,std::ios::binary); f<<"VALID_ORIGINAL"; }
    World world; world.create("Good");
    auto invalid=SceneSerializer::capture(world,"Good");
    SerializedComponentData broken;
    broken.type=ComponentRegistry::stable_id("Tests.Broken");
    broken.version=1;
    broken.type_name="Tests.Broken";
    SerializedPropertyData bad_property;
    bad_property.name="Missing";
    bad_property.kind=PropertyKind::EntityReference;
    bad_property.value=Entity::make(0,0);
    broken.properties.push_back(bad_property);
    invalid.objects.front().components.push_back(broken);
    std::string error;
    check(!SceneSerializer::save_file(invalid,destination,&error) &&
          contents(destination)=="VALID_ORIGINAL" && !error.empty(),
          "CORE-011: failed serialization preserves original bytes");
    auto valid=SceneSerializer::capture(world,"Good");
    check(!SceneSerializer::save_file(valid,root/"missing-parent"/"new.nscene",&error) &&
          contents(destination)=="VALID_ORIGINAL",
          "CORE-011: unwritable destination does not truncate existing Scene");
    std::size_t temp_count=0;
    for(auto& entry:std::filesystem::directory_iterator(root))
        if(entry.path().filename().string().find(".nengine-tmp-")!=std::string::npos) ++temp_count;
    check(temp_count==0,"CORE-011: failure cleans temporary file");
    check(SceneSerializer::save_file(valid,destination,&error) &&
          contents(destination).find("NENGINE_SCENE")==0,
          "CORE-011: successful save atomically replaces previous file");
    SceneData restored;
    check(SceneSerializer::load_file(destination,restored,&error) &&
          restored.name=="Good","CORE-011: published file round-trips");
    std::filesystem::remove_all(root,ec);
}
}
int main() {
    play_isolation();scene_switch();atomic_saves();
    if(failures)std::cerr<<failures<<" Core acceptance failures\n";
    else std::cout<<"RoundTwoCoreAcceptanceTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
