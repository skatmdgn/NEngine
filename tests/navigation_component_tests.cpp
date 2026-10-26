#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include "nengine/editor/editor_model.hpp"
#include "nengine/navigation/components.hpp"
#include "nengine/navigation/nav_scene_agent.hpp"
#include "nengine/navigation/registration.hpp"
#include "nengine/core/scene.hpp"
using namespace nengine;
namespace {
int failures=0;
void check(bool b,const char* s){if(!b){++failures;std::cerr<<"FAIL "<<s<<"\n";}}
std::vector<core::Vec3> square(float x){
    return {{x,0,0},{x+1,0,0},{x+1,0,1},{x,0,1}};
}
}
int main(){
    editor::EditorModel editor;
    check(editor.component_registry().find(navigation::nav_agent_type())!=nullptr &&
          editor.component_factories().contains(navigation::nav_agent_type()),
          "NAV-010: Scene agent appears in Editor component catalog");
    auto e=editor.world().create("Navigating Actor");
    check(editor.component_factories().add(editor.world(),e,navigation::nav_agent_type()),
          "NAV-010: Inspector component factory attaches agent to Scene");
    const auto changed=editor.property_access().write(
        editor.world(),e,navigation::nav_agent_type(),"Destination",
        core::PropertyValue{core::Vec3{1.8f,0,.5f}});
    check(changed,"NAV-010: Inspector destination changes native agent data");
    auto* agent=editor.world().get_component<navigation::NavAgentComponent>(
        e,navigation::nav_agent_type());
    check(agent!=nullptr,"NAV-010: authored agent component remains attached");
    agent->has_destination=true;
    editor.world().transform(e)->local_position={.2f,0,.5f};
    core::SceneData scene;
    std::string error;
    check(core::SceneSerializer::capture_checked(
          editor.world(),scene,"Navigation",&error,
          &editor.component_serialization()),
          "NAV-010: Scene saves agent destination and movement settings");
    core::World loaded;
    check(core::SceneSerializer::instantiate(
          scene,loaded,&error,&editor.component_serialization()),
          "NAV-010: Scene restores agent component");
    const auto entities=loaded.entities();
    check(entities.size()==1,"NAV-010: roundtrip has one entity");
    auto* copy=loaded.get_component<navigation::NavAgentComponent>(
        entities[0],navigation::nav_agent_type());
    check(copy&&copy->has_destination&&copy->target==core::Vec3{1.8f,0,.5f} &&
          !copy->runtime_initialized,
          "NAV-010: Scene persists intent, never runtime path cache");
    navigation::NavMesh mesh;
    check(mesh.add_polygon(square(0))&&mesh.add_polygon(square(1))&&
          mesh.connect(0,1),
          "NAV-010: build routable Player navigation graph");
    const auto before=loaded.transform(entities[0])->local_position.x;
    check(navigation::step_scene_agents(loaded,mesh,.25f)==1u &&
          loaded.transform(entities[0])->local_position.x>before,
          "NAV-010: component drives actual Scene Transform on fixed update");
    for(int i=0;i<30;++i)navigation::step_scene_agents(loaded,mesh,.1f);
    check(!copy->has_destination,
          "NAV-010: movement completes at authored destination");
    auto damaged=scene;
    for(auto& field:damaged.objects[0].components[0].properties){
        if(field.name=="Speed")field.value=std::numeric_limits<double>::quiet_NaN();
    }
    const auto count=loaded.size();
    check(!core::SceneSerializer::instantiate(damaged,loaded,&error,
          &editor.component_serialization()) && loaded.size()==count,
          "NAV-010: malformed agent Scene rejects transactionally");
    if(failures)return EXIT_FAILURE;
    std::cout<<"NEngineNavigationComponentTests PASS\n";
    return EXIT_SUCCESS;
}
