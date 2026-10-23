#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include "nengine/core/scene.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"

namespace {
int failures=0;
void check(bool yes,const char* why){
    if(!yes){++failures;std::cerr<<"FAIL: "<<why<<"\n";}
}
std::string read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
int main(){
    using namespace nengine;
    std::error_code ec;
    const auto root=std::filesystem::temp_directory_path()/"nengine-player-safety-20261023";
    std::filesystem::remove_all(root,ec);
    auto project=root/"project",outputs=root/"exports";
    std::filesystem::create_directories(project/"Assets"/"Scenes",ec);
    std::filesystem::create_directories(outputs,ec);
    const auto scene_path=project/"Assets"/"Scenes"/"Main.nscene";
    core::World world;world.create("Working Scene");
    const auto captured=core::SceneSerializer::capture(world,"Working Scene");
    std::string error;
    check(core::SceneSerializer::save_file(captured,scene_path,&error),
          "WIN-014: valid startup Scene generated");
    player::ExportSettings settings;
    settings.project_root=project;
    settings.startup_scene="Assets/Scenes/Main.nscene";
    settings.destination=outputs/"Good";
    settings.copy_supported_assets=false;
    player::ExportReport report;
    check(player::export_player_data(settings,report,&error),
          "WIN-014: valid bundle atomically publishes to target directory");
    player::PlayerRuntime runtime;
    check(runtime.load(settings.destination,&error)&&runtime.ready()&&
          runtime.entity_count()==1,
          "WIN-010: valid Player bundle loads before injecting errors");
    const auto old_count=runtime.entity_count();
    // Every error must produce a diagnostic and preserve a working Player.
    const auto manifest=settings.destination/"NEngine.player";
    const auto original_manifest=read(manifest);
    {std::ofstream out(manifest,std::ios::binary|std::ios::trunc);
     out<<"INVALID BUNDLE";}
    error.clear();
    check(!runtime.load(settings.destination,&error)&&
          runtime.ready()&&runtime.entity_count()==old_count&&!error.empty(),
          "WIN-010: corrupted manifest diagnosed, previous Player World preserved");
    {std::ofstream out(manifest,std::ios::binary|std::ios::trunc);
     out<<original_manifest;}
    const auto exported_scene=settings.destination/"Content"/"Main.nscene";
    const auto original_scene=read(exported_scene);
    {std::ofstream out(exported_scene,std::ios::binary|std::ios::trunc);
     out<<"BROKEN SCENE";}
    error.clear();
    check(!runtime.load(settings.destination,&error)&&
          runtime.ready()&&runtime.entity_count()==old_count&&!error.empty(),
          "WIN-010: corrupted Scene diagnosed without destroying loaded World");
    {std::ofstream out(exported_scene,std::ios::binary|std::ios::trunc);
     out<<original_scene;}
    check(runtime.load(settings.destination,&error)&&runtime.ready(),
          "WIN-010: after source recovery Player can reload");

    settings.destination=outputs/"Existing";
    std::filesystem::create_directories(settings.destination,ec);
    const auto sentinel=settings.destination/"SaveData.keep";
    {std::ofstream out(sentinel,std::ios::binary);out<<"DO_NOT_DELETE";}
    error.clear();
    check(!player::export_player_data(settings,report,&error)&&
          read(sentinel)=="DO_NOT_DELETE"&&!error.empty(),
          "WIN-014: export collision cannot overwrite existing user files");
    settings.destination=outputs/"BadSource";
    {std::ofstream out(scene_path,std::ios::binary|std::ios::trunc);
     out<<"MALFORMED SCENE";}
    error.clear();
    check(!player::export_player_data(settings,report,&error)&&
          !std::filesystem::exists(settings.destination)&&!error.empty(),
          "WIN-014: malformed source refuses publish and reports reason");
    {std::ofstream out(scene_path,std::ios::binary|std::ios::trunc);
     out<<read(exported_scene);}
    settings.destination=outputs/"MissingPlayerExecutable";
    settings.player_executable=root/"not-existing-player.exe";
    error.clear();
    check(!player::export_player_data(settings,report,&error)&&
          !std::filesystem::exists(settings.destination)&&!error.empty(),
          "WIN-014: missing binary rejects output with no partial bundle");
    bool any_staging=false;
    for(const auto& entry:std::filesystem::directory_iterator(outputs))
       if(entry.path().filename().string().find(".nengine")!=std::string::npos ||
          entry.path().filename().string().find(".tmp")!=std::string::npos)
          any_staging=true;
    check(!any_staging,"WIN-014: failed export cleans staging files");
    runtime.unload();
    check(!runtime.ready()&&runtime.entity_count()==0,
          "WIN-010: safe explicit exit after recovery");
    std::filesystem::remove_all(root,ec);
    if(!failures)std::cout<<"NEngineRoundThreePlayerSafetyTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
