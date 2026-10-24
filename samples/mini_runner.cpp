// Two real, portable sample games: the same project/Scene/PlayerRuntime path
// runs with 2D or 3D physics, controls, a camera and an explicit win condition.
// Launch with --play for console controls; CI uses --self-test.
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/input/action_map_io.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"

#ifndef NENGINE_SAMPLE_2D
#define NENGINE_SAMPLE_2D 1
#endif

namespace {
using namespace nengine;
constexpr bool game_2d=NENGINE_SAMPLE_2D!=0;

bool create_project(const std::filesystem::path& project,std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(project/"Assets"/"Scenes",ec);
    if(ec){error="Cannot initialize project directory";return false;}
    core::World world;
    auto camera=world.create("Main Camera");
    auto* camera_component=world.add_component<render::Camera>(
        camera,render::camera_type());
    if(!camera_component)return false;
    camera_component->projection=game_2d?render::ProjectionMode::Orthographic
                                        :render::ProjectionMode::Perspective;
    camera_component->orthographic_size=5.f;
    auto runner=world.create("Player");
    auto* body=game_2d
        ? nullptr
        : world.add_component<physics::Rigidbody>(
            runner,physics::rigidbody_type());
    auto* body2d=game_2d
        ? world.add_component<physics::Rigidbody2D>(
            runner,physics::rigidbody2d_type())
        : nullptr;
    if(!body&&!body2d)return false;
    if(body) {body->use_gravity=false;body->allow_sleep=false;}
    if(body2d){body2d->use_gravity=false;body2d->allow_sleep=false;}
    if(game_2d){
        if(!world.add_component<render::SpriteRenderer>(
               runner,render::sprite_renderer_type())||
           !world.add_component<physics::BoxCollider2D>(
               runner,physics::box_collider2d_type()))return false;
    }else{
        if(!world.add_component<render::MeshRenderer>(
               runner,render::mesh_renderer_type())||
           !world.add_component<physics::BoxCollider>(
               runner,physics::box_collider_type()))return false;
    }
    auto goal=world.create("Finish Line");
    world.transform(goal)->local_position={3.0f,0.0f,0.0f};
    core::ComponentSerializationRegistry codecs;
    if(!render::register_component_serializers(codecs) ||
       !physics::register_component_serializers(codecs))return false;
    core::SceneData scene;
    if(!core::SceneSerializer::capture_checked(
        world,scene,game_2d?"Mini Runner 2D":"Mini Runner 3D",&error,&codecs))
        return false;
    if(!core::SceneSerializer::save_file(
       scene,project/"Assets"/"Scenes"/"Main.nscene",&error))return false;
    input::ActionMap actions;
    if(!actions.bind("MoveRight",input::Key::D) ||
       !actions.bind("MoveLeft",input::Key::A) ||
       !input::save_action_map_file(project/"Input.ninput",actions,&error))
        return false;
    return true;
}
bool simulate(player::PlayerRuntime& runtime,bool move_right,
              player::PlayerFrameStats& last) {
    runtime.begin_input_frame();
    runtime.input().set_key(input::Key::D,move_right);
    runtime.input().set_key(input::Key::A,!move_right);
    if(!runtime.actions().contains("MoveRight") ||
       !runtime.actions().contains("MoveLeft"))return false;
    auto player=core::Entity::invalid();
    for(auto e:runtime.world().entities())
        if(runtime.world().name(e)=="Player"){player=e;break;}
    if(!player.valid())return false;
    const float vx=runtime.actions().held("MoveRight",runtime.input())?3.f:
       runtime.actions().held("MoveLeft",runtime.input())?-3.f:0.f;
    if(game_2d){
        auto* body=runtime.world().get_component<physics::Rigidbody2D>(
            player,physics::rigidbody2d_type());
        if(!body)return false;
        body->linear_velocity={vx,0,0};
    }else{
        auto* body=runtime.world().get_component<physics::Rigidbody>(
            player,physics::rigidbody_type());
        if(!body)return false;
        body->linear_velocity={vx,0,0};
    }
    last=runtime.advance(1.0f/60.0f);
    return last.physics_steps>0 &&
        (game_2d?last.integrated_2d>0:last.integrated_3d>0);
}
bool won(const player::PlayerRuntime& runtime) {
    for(auto e:runtime.world().entities())
        if(runtime.world().name(e)=="Player")
            return runtime.world().transform(e)->local_position.x>=3.f;
    return false;
}
}

int main(int argc,char** argv) {
    if(argc!=2 || (std::string(argv[1])!="--self-test" &&
                    std::string(argv[1])!="--play")){
        std::cerr<<"Usage: NEngineMiniRunner" <<(game_2d?"2D":"3D")
                 <<" --self-test | --play\n";
        return 2;
    }
    const bool automatic=std::string(argv[1])=="--self-test";
    const auto root=std::filesystem::temp_directory_path()/
        (game_2d?"nengine-minigame-2d":"nengine-minigame-3d");
    std::error_code ec;
    std::filesystem::remove_all(root,ec);
    const auto project=root/"Project";
    std::string error;
    if(!create_project(project,error)) {
        std::cerr<<"Sample project creation failed: "<<error<<"\n";return 3;
    }
    player::ExportSettings settings;
    settings.project_root=project;
    settings.startup_scene="Assets/Scenes/Main.nscene";
    settings.destination=root/"ExportedGame";
    settings.copy_supported_assets=false;
    settings.title=game_2d?"Mini Runner 2D":"Mini Runner 3D";
    player::ExportReport report;
    if(!player::export_player_data(settings,report,&error)){
        std::cerr<<"Sample export failed: "<<error<<"\n";return 4;
    }
    player::PlayerRuntime runtime;
    if(!runtime.load(report.bundle_root,&error)||!runtime.ready()||
       runtime.entity_count()!=3){
        std::cerr<<"Sample Player load failed: "<<error<<"\n";return 5;
    }
    bool saw_step=false;
    player::PlayerFrameStats stats{};
    const int frames=automatic?120:10000;
    for(int i=0;i<frames&&!won(runtime);++i) {
        bool right=true;
        if(!automatic){
            char c;
            if(!(std::cin>>c) || c=='q')break;
            right=c!='a';
        }
        if(!simulate(runtime,right,stats)){
            std::cerr<<"Sample Player simulation failed\n";return 6;
        }
        saw_step=true;
    }
    if(automatic && (!won(runtime)||!saw_step||
        runtime.elapsed_seconds()<0.99 ||
        runtime.actions().action_count()!=2)){
        std::cerr<<"Sample did not reach goal or lost its packaged state\n";return 7;
    }
    if(won(runtime))std::cout<<"YOU WIN - "<<settings.title<<"\n";
    else std::cout<<"GAME QUIT - "<<settings.title<<"\n";
    runtime.unload();
    std::filesystem::remove_all(root,ec);
    return 0;
}
