#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "nengine/core/scene.hpp"
#include "nengine/audio/components.hpp"
#include "nengine/audio/registration.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/scripting/components.hpp"
#include "nengine/scripting/registration.hpp"
#include "nengine/navigation/components.hpp"
#include "nengine/navigation/registration.hpp"

using namespace nengine;
int main(){
    core::ComponentSerializationRegistry codecs;
    if(!render::register_component_serializers(codecs)||
       !physics::register_component_serializers(codecs)||
       !audio::register_component_serializers(codecs)||
       !scripting::register_component_serializers(codecs)||
       !navigation::register_component_serializers(codecs))
        return 1;
    core::World authored;
    struct Kind {const char* name;core::ComponentTypeId type;};
    std::vector<Kind> ids;
    const auto guid=assets::AssetGuid::generate();
    const auto add=[&](auto tag,const char* name,core::ComponentTypeId type){
        const auto e=authored.create(name);
        using T=decltype(tag);
        auto* component=authored.add_component<T>(e,type);
        if(!component)return static_cast<T*>(nullptr);
        ids.push_back({name,type});
        authored.transform(e)->local_position={
            float(ids.size()),float(ids.size())*.25f,0};
        return component;
    };
    auto* cam=add(render::Camera{},"Camera",render::camera_type());
    auto* light=add(render::Light{},"Light",render::light_type());
    auto* mesh=add(render::MeshRenderer{},"Mesh",render::mesh_renderer_type());
    auto* sprite=add(render::SpriteRenderer{},"Sprite",render::sprite_renderer_type());
    auto* anim=add(render::SpriteAnimator{},"Animator",render::sprite_animator_type());
    auto* body=add(physics::Rigidbody{},"Physics3D",physics::rigidbody_type());
    auto* box=add(physics::BoxCollider{},"Box3D",physics::box_collider_type());
    auto* sphere=add(physics::SphereCollider{},"Sphere3D",physics::sphere_collider_type());
    auto* capsule=add(physics::CapsuleCollider{},"Capsule3D",physics::capsule_collider_type());
    auto* body2=add(physics::Rigidbody2D{},"Physics2D",physics::rigidbody2d_type());
    auto* box2=add(physics::BoxCollider2D{},"Box2D",physics::box_collider2d_type());
    auto* circle=add(physics::CircleCollider2D{},"Circle2D",physics::circle_collider2d_type());
    auto* capsule2=add(physics::CapsuleCollider2D{},"Capsule2D",physics::capsule_collider2d_type());
    auto* sound=add(audio::AudioSource{},"Audio",audio::audio_source_type());
    auto* listener=add(audio::AudioListener{},"Listener",audio::audio_listener_type());
    auto* script=add(scripting::ScriptBehaviour{},"Script",scripting::script_behaviour_type());
    auto* agent=add(navigation::NavAgentComponent{},"Agent",navigation::nav_agent_type());
    if(!cam||!light||!mesh||!sprite||!anim||!body||!box||!sphere||
       !capsule||!body2||!box2||!circle||!capsule2||!sound||!listener||
       !script||!agent)return 2;
    cam->vertical_fov_degrees=71.f;
    light->intensity=3.f;
    mesh->mesh=guid;mesh->material=guid;
    sprite->texture=guid;sprite->sort_order=7;
    anim->clip=guid;anim->speed=2.f;
    body->continuous=true;body->mass=3.f;
    box->size={2,3,4};
    sphere->radius=2.f;
    capsule->height=3.f;
    body2->continuous=true;body2->mass=2.f;
    box2->size={2,4,0};
    circle->radius=1.5f;
    capsule2->size={1,3,0};
    sound->clip=guid;sound->volume=.3f;
    listener->volume=.7f;
    script->type_name="Game.Player";
    agent->has_destination=true;agent->target={4,0,2};
    core::SceneData initial;
    std::string error;
    if(!core::SceneSerializer::capture_checked(authored,initial,
               "All Components",&error,&codecs)){
        std::cerr<<error;return 3;
    }
    std::stringstream data;
    if(!core::SceneSerializer::write(initial,data,&error))return 4;
    core::SceneData parsed;
    if(!core::SceneSerializer::read(data,parsed,&error))return 5;
    core::World loaded;
    if(!core::SceneSerializer::instantiate(parsed,loaded,&error,&codecs)){
        std::cerr<<error;return 6;
    }
    if(loaded.size()!=ids.size())return 7;
    for(const auto& id:ids){
        bool found=false;
        for(auto e:loaded.entities()){
            if(loaded.name(e)==id.name&&loaded.has_component(e,id.type))
                found=true;
        }
        if(!found){std::cerr<<"Missing "<<id.name;return 8;}
    }
    core::SceneData replay;
    if(!core::SceneSerializer::capture_checked(loaded,replay,
             "All Components",&error,&codecs))return 9;
    std::stringstream first,second;
    if(!core::SceneSerializer::write(initial,first,&error)||
       !core::SceneSerializer::write(replay,second,&error)||
       first.str()!=second.str())return 10;
    // Explicitly exercise new continuous Rigidbody persistence as a property
    // that may otherwise be silently dropped by a serialization schema.
    bool continuous3=false,continuous2=false;
    for(const auto e:loaded.entities()){
        if(auto* rb=loaded.get_component<physics::Rigidbody>(
             e,physics::rigidbody_type()))continuous3=rb->continuous;
        if(auto* rb=loaded.get_component<physics::Rigidbody2D>(
             e,physics::rigidbody2d_type()))continuous2=rb->continuous;
    }
    if(!continuous3||!continuous2)return 11;
    std::cout<<"NEngineCoreComponentRoundtripTests PASS: "
             <<ids.size()<<" registered component types\n";
    return EXIT_SUCCESS;
}
