#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/input/action_map_io.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"

namespace {
int failures=0;
void expect(bool success,const char* msg){
    if(!success){++failures;std::cerr<<"FAIL: "<<msg<<"\n";}
}
}

int main(){
    using namespace nengine;
    std::error_code ec;
    const auto base=std::filesystem::temp_directory_path()/
        "nengine-input-unicode-acceptance-20261023";
    std::filesystem::remove_all(base,ec);
    const auto project=base/std::filesystem::u8path(u8"\uD55C\uAE00_\uD504\uB85C\uC81D\uD2B8");
    const auto scene=project/"Assets"/std::filesystem::u8path(u8"\uC7A5\uBA74")/"Main.nscene";
    const auto out=base/std::filesystem::u8path(u8"\uCD9C\uB825_\uD30C\uC77C");
    std::filesystem::create_directories(scene.parent_path(),ec);
    std::filesystem::create_directories(out,ec);
    core::World authoring;authoring.create("Game");
    std::string error;
    auto data=core::SceneSerializer::capture(authoring,"Gameplay");
    expect(core::SceneSerializer::save_file(data,scene,&error),
           "WIN-013: writes a Scene in Korean UTF-8 directory");
    input::ActionMap actions;
    actions.bind("Jump",input::Key::Space);
    expect(input::save_action_map_file(project/"Input.ninput",actions,&error),
           "INPUT-004: persists authoring Action Map");
    player::ExportSettings config;
    config.project_root=project;
    config.startup_scene=std::filesystem::path("Assets")/
        std::filesystem::u8path(u8"\uC7A5\uBA74")/"Main.nscene";
    config.copy_supported_assets=false;
    config.destination=out/"First";
    player::ExportReport report;
    expect(player::export_player_data(config,report,&error),
           "INPUT-004/WIN-013: export bundle with custom Action Map from Korean project");
    player::PlayerRuntime first;
    expect(first.load(report.bundle_root,&error)&&first.ready()&&first.entity_count()==1,
           "WIN-013: compiled Player runtime loads Korean path project bundle");
    first.begin_input_frame();
    first.input().set_key(input::Key::Space,true);
    expect(first.actions().pressed("Jump",first.input()),
           "INPUT-004: first Player executes bound Space action");
    // Change the Editor-side mapping, export a new immutable bundle, and
    // verify the new Player differs without silently mutating the old one.
    actions.unbind("Jump");
    actions.bind("Jump",input::Key::F);
    expect(input::save_action_map_file(project/"Input.ninput",actions,&error,true),
           "INPUT-004: save modified Editor Action Map");
    config.destination=out/"Second";
    expect(player::export_player_data(config,report,&error),
           "INPUT-004: export newly edited Action Map in independent bundle");
    player::PlayerRuntime updated;
    expect(updated.load(report.bundle_root,&error)&&updated.ready(),
           "INPUT-004: load edited Player bundle");
    updated.begin_input_frame();
    updated.input().set_key(input::Key::F,true);
    expect(updated.actions().pressed("Jump",updated.input())&&
           !updated.actions().held("Jump",first.input())&&
           first.actions().held("Jump",first.input()),
           "INPUT-004: new F mapping active while older exported bundle retains Space mapping");
    // Export should never publish an invalid or partial input map.
    {std::ofstream broken(project/"Input.ninput",std::ios::binary|std::ios::trunc);
     broken<<"INVALID ACTION MAP";}
    config.destination=out/"Broken";
    error.clear();
    expect(!player::export_player_data(config,report,&error)&&
           !std::filesystem::exists(config.destination)&&!error.empty(),
           "INPUT-004: corrupt authoring map fails export transactionally");
    std::filesystem::remove_all(base,ec);
    if(!failures)std::cout<<"NEngineInputBundleAcceptanceTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
