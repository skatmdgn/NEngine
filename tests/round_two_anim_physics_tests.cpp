// Round 2 acceptance: ANIM-004, ANIM-010, PHYS-009, PHYS-012.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include "nengine/render/animation_controller.hpp"
#include "nengine/render/sprite_animation.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/physics/simulation.hpp"

namespace {
int failures=0;
void check(bool b,const char* msg){if(!b){++failures;std::cerr<<"FAIL: "<<msg<<"\n";}}
void animation_transitions() {
    using namespace nengine;
    render::AnimationController ctrl;
    auto idle=assets::AssetGuid::generate(),run=assets::AssetGuid::generate();
    ctrl.default_state="Idle";
    ctrl.states={{"Idle",idle,true,1.0f},{"Run",run,true,1.5f}};
    ctrl.transitions={{"Idle","Run","move",true,0.5f},
                      {"Run","Idle","move",false,0.0f}};
    std::string error;
    check(ctrl.validate(&error),"ANIM-004: valid state machine accepted");
    render::AnimationControllerPlayer player;
    check(player.evaluate(ctrl,0)&&player.state=="Idle",
          "ANIM-004: entering default state");
    player.set_bool("move",true);
    check(!player.evaluate(ctrl,0.49f)&&player.state=="Idle",
          "ANIM-004: exit time blocks early transition");
    check(player.evaluate(ctrl,0.5f)&&player.state=="Run" &&
          !player.evaluate(ctrl,0.0f),
          "ANIM-004: threshold moves to Run exactly once");
    player.set_bool("move",false);
    check(player.evaluate(ctrl,0)&&player.state=="Idle",
          "ANIM-004: opposite parameter moves back to Idle");
    ctrl.transitions.push_back({"Missing","Run","x",true,0});
    check(!ctrl.validate(&error)&&!error.empty(),
          "ANIM-004: invalid state reference rejected");
}
void animation_endurance() {
    using namespace nengine;
    render::SpriteAnimationClip clip;
    clip.frames={{assets::AssetGuid::generate(),0.1f},
                 {assets::AssetGuid::generate(),0.2f},
                 {assets::AssetGuid::generate(),0.3f}};
    check(clip.valid(),"ANIM-010: generated clip valid");
    for(int i=0;i<10000;++i) {
        const auto t=static_cast<float>(i)*0.125f;
        const auto idx=render::sample_sprite_animation_frame(clip,t,true);
        check(idx.has_value()&&*idx<3,
              "ANIM-010: long looping playback never escapes frame bounds");
        if(failures)break;
    }
    check(render::sample_sprite_animation_frame(clip,99.0f,false)==2,
          "ANIM-010: non-looping animation stops at final frame");
    check(render::sample_sprite_animation_frame(clip,0.0f,true)==0 &&
          render::sample_sprite_animation_frame(clip,-1.0f,true)==0,
          "ANIM-010: loop restart and negative time clamp");
    render::AnimationController controller;
    controller.default_state="A";
    controller.states={{"A",assets::AssetGuid::generate(),true,1.0f}};
    render::AnimationControllerPlayer player;
    player.evaluate(controller,0);
    player.set_bool("start",true);
    player.reset();
    check(player.state.empty()&&!player.get_bool("start") &&
          player.evaluate(controller,0)&&player.state=="A",
          "ANIM-010: state reset/restart clears stale parameters");
    check(!render::sample_sprite_animation_frame(
          clip,std::numeric_limits<float>::infinity(),true),
          "ANIM-010: invalid time rejected");
}
void filtering() {
    using namespace nengine;
    core::World world;
    const auto left=world.create("Layer0");
    const auto right=world.create("Layer3");
    world.transform(left)->local_position={2,0,0};
    world.transform(right)->local_position={5,0,0};
    auto* a=world.add_component<physics::BoxCollider>(left,physics::box_collider_type());
    // Adding the next collider can relocate the component pool.
    // Set fields before insertion and do not retain the earlier pointer.
    a->layer=0;
    auto* b=world.add_component<physics::BoxCollider>(right,physics::box_collider_type());
    b->layer=3;
    auto mask3=std::uint32_t{1}<<3;
    auto mask0=std::uint32_t{1}<<0;
    auto zero=physics::overlap_box(world,{2,0,0},{2,2,2},false,mask0);
    auto three=physics::overlap_box(world,{5,0,0},{2,2,2},false,mask3);
    check(zero.size()==1&&zero.front()==left && three.size()==1&&three.front()==right,
          "PHYS-009: overlap layer filters select matching shape only");
    auto ray=physics::raycast(world,{0,0,0},{1,0,0},10,false,mask3);
    check(ray&&ray->entity==right&&ray->layer==3,
          "PHYS-009: raycast uses same layer mask");
    auto none=physics::overlap_box(world,{2,0,0},{20,2,2},false,0);
    check(none.empty(),"PHYS-009: zero mask matches nothing");
    b->is_trigger=true;
    auto excluded=physics::raycast(world,{0,0,0},{1,0,0},10,false,mask3);
    auto included=physics::raycast(world,{0,0,0},{1,0,0},10,true,mask3);
    check(!excluded&&included&&included->is_trigger,
          "PHYS-009: trigger inclusion filtering matches cast");
}
void separate_domains() {
    using namespace nengine;
    core::World world;
    const auto e3=world.create("3D");
    const auto e2=world.create("2D");
    world.add_component<physics::BoxCollider>(e3,physics::box_collider_type());
    world.add_component<physics::BoxCollider2D>(e2,physics::box_collider2d_type());
    auto* body3=world.add_component<physics::Rigidbody>(e3,physics::rigidbody_type());
    auto* body2=world.add_component<physics::Rigidbody2D>(e2,physics::rigidbody2d_type());
    body3->linear_velocity={1,0,0};
    body2->linear_velocity={0,1,0};
    body3->use_gravity=false;
    body2->use_gravity=false;
    auto collisions=physics::detect_box_overlaps(world);
    check(collisions.overlaps.empty(),
          "PHYS-012: overlapping 2D and 3D bodies cannot collide cross-dimensionally");
    auto updated=physics::step_rigidbodies(world,0.1f,{0,0,0});
    check(updated.integrated_3d==1&&updated.integrated_2d==1,
          "PHYS-012: separate 2D and 3D integrators advance each body once");
    check(world.transform(e3)->local_position.x>0.05f&&
          world.transform(e2)->local_position.y>0.05f,
          "PHYS-012: each dimension advances independently");
    check(physics::overlap_box(world,{0,0,0},{20,20,20}).size()==1&&
          physics::overlap_box_2d(world,{0,0},{20,20}).size()==1,
          "PHYS-012: queries never return objects from the other physics dimension");
}
}
int main(){
 animation_transitions();animation_endurance();filtering();separate_domains();
 if(!failures)std::cout<<"RoundTwoAnimPhysicsAcceptanceTests PASS\n";
 return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
