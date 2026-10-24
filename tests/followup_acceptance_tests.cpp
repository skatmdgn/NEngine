#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <limits>

#include "nengine/core/scene.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/registration.hpp"

namespace {
int failures=0;
void check(bool value,const char* message){
    if(!value){++failures;std::cerr<<"FAIL "<<message<<"\n";}
}
bool near(float a,float b) {return std::fabs(a-b)<0.00001f;}
void pixel_camera() {
    using namespace nengine;
    core::World world;
    auto e=world.create("Pixel Camera");
    auto* camera=world.add_component<render::Camera>(e,render::camera_type());
    camera->projection=render::ProjectionMode::Orthographic;
    camera->pixel_perfect=true;
    camera->pixels_per_unit=100;
    world.transform(e)->local_position={0.012f,-0.018f,0};
    auto pixel=render::build_camera_matrices(world,e,16.f/9.f,1080);
    check(pixel && near(pixel->world.at(0,3),0.01f) &&
          near(pixel->world.at(1,3),-0.02f) &&
          near(pixel->projection.at(1,1),-1.0f/5.4f),
          "TWOD-006: 1080p pixel-perfect orthographic projection and pixel snapping");
    camera->pixel_perfect=false;
    auto plain=render::build_camera_matrices(world,e,16.f/9.f,1080);
    check(plain && near(plain->world.at(0,3),0.012f) &&
          near(plain->projection.at(1,1),-0.2f),
          "TWOD-006: nonpixel 2D camera is unchanged");
    camera->pixel_perfect=true;
    auto resized=render::build_camera_matrices(world,e,16.f/9.f,720);
    check(resized&&near(resized->projection.at(1,1),-1.0f/3.6f),
          "TWOD-006: camera preserves actual pixel density after viewport resize");
    core::ComponentSerializationRegistry codecs;
    check(render::register_component_serializers(codecs),
          "TWOD-006: Camera component codec installed");
    core::SceneData captured;
    std::string error;
    check(core::SceneSerializer::capture_checked(world,captured,"Pixel",&error,&codecs),
          "TWOD-006: Pixel Perfect config captured into Scene");
    core::World restored;
    check(core::SceneSerializer::instantiate(captured,restored,&error,&codecs) &&
          restored.size()==1,"TWOD-006: Scene camera restore");
    if(restored.size()==1){
        auto* value=restored.get_component<render::Camera>(
            restored.entities()[0],render::camera_type());
        check(value&&value->pixel_perfect&&near(value->pixels_per_unit,100),
              "TWOD-006: settings persist through Scene");
    }
    auto invalid=captured;
    for(auto& prop:invalid.objects[0].components[0].properties)
        if(prop.name=="Pixels Per Unit")
            prop.value=std::numeric_limits<double>::quiet_NaN();
    check(!core::SceneSerializer::instantiate(invalid,restored,&error,&codecs)&&
          restored.size()==1,
          "TWOD-006: malformed pixel camera rejected transactionally");
}
void managed_diagnostics() {
    using namespace nengine;
    std::error_code ec;
    const auto root=std::filesystem::temp_directory_path()/
        "nengine-managed-name-diagnostic-20261024";
    std::filesystem::remove_all(root,ec);
    const auto scene_path=root/"Project"/"Assets"/"Scenes"/"Main.nscene";
    std::filesystem::create_directories(scene_path.parent_path(),ec);
    core::World world;world.create("Host");
    std::string error;
    check(core::SceneSerializer::save_file(core::SceneSerializer::capture(world),
          scene_path,&error),"CS-012: create Scene fixture");
    player::ExportSettings settings;
    settings.project_root=root/"Project";
    settings.startup_scene="Assets/Scenes/Main.nscene";
    settings.destination=root/"Export";
    settings.copy_supported_assets=false;
    player::ExportReport report;
    check(player::export_player_data(settings,report,&error),
          "CS-012: export baseline bundle without managed assemblies");
    player::PlayerRuntime runner;
    check(runner.load(settings.destination,&error),
          "CS-012: load baseline native player");
    error.clear();
    check(!runner.initialize_managed(&error) &&
          error.find("GameScripts.dll")!=std::string::npos &&
          error.find("assembly name mismatch")!=std::string::npos,
          "CS-012: missing/renamed GameScripts DLL names expected file and solution");
    auto managed=settings.destination/"Content"/"Managed";
    std::filesystem::create_directories(managed,ec);
    {std::ofstream f(managed/"GameScripts.dll",std::ios::binary);f<<"dummy";}
    error.clear();
    check(!runner.initialize_managed(&error) &&
          error.find("GameScripts.runtimeconfig.json")!=std::string::npos,
          "CS-012: missing runtimeconfig reports exact expected name");
    {std::ofstream f(managed/"GameScripts.runtimeconfig.json");
     f<<"{\\"runtimeOptions\\":{\\"tfm\\":\\"net8.0\\"}}";}
    error.clear();
    check(!runner.initialize_managed(&error) &&
          error.find("NEngine.Bridge.dll")!=std::string::npos,
          "CS-012: missing Bridge names exact dependency");
    {std::ofstream f(managed/"NEngine.Bridge.dll",std::ios::binary);f<<"dummy";}
    error.clear();
    check(!runner.initialize_managed(&error) &&
          error.find("NEngine.API.dll")!=std::string::npos,
          "CS-012: missing API names exact dependency");
    std::filesystem::remove_all(root,ec);
}
}
int main(){
    pixel_camera();
    managed_diagnostics();
    if(!failures)std::cout<<"NEngineFollowupAcceptanceTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
