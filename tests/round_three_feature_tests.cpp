#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "nengine/core/scene.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/simulation.hpp"
#include "nengine/render/tilemap.hpp"
#include "nengine/render/particle.hpp"
#include "nengine/render/registration.hpp"

namespace {
int failures = 0;
void check(bool ok, const char* message) {
    if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
bool near(float value,float target,float tolerance=0.002f){
    return std::isfinite(value)&&std::fabs(value-target)<=tolerance;
}
void continuous_casts() {
    using namespace nengine;
    core::World world;
    const auto box=world.create("Wall");
    world.transform(box)->local_position={5,0,0};
    world.add_component<physics::BoxCollider>(box,physics::box_collider_type());
    auto hit=physics::sphere_cast(world,{0,0,0},.5f,{1,0,0},20);
    check(hit&&hit->entity==box&&near(hit->distance,4),
        "PHYS-004: sphere cast against box returns first contact at 4 meters");
    check(!physics::sphere_cast(world,{0,0,0},.5f,{1,0,0},3),
        "PHYS-004: maximum range rejects targets");
    check(!physics::sphere_cast(world,{0,0,0},-1,{1,0,0},10) &&
          !physics::sphere_cast(world,{0,0,0},1,{0,0,0},10) &&
          !physics::sphere_cast(world,{0,0,0},1,{1,0,0},-1) &&
          !physics::sphere_cast(world,{0,0,0},1,
                               {std::numeric_limits<float>::quiet_NaN(),0,0},20),
        "PHYS-004: invalid cast inputs rejected");
    auto capsule=physics::capsule_cast(world,{0,0,0},.5f,3.0f,{1,0,0},20);
    check(capsule&&capsule->entity==box&&near(capsule->distance,4),
          "PHYS-005: upright capsule casts into wall at correct distance");
    check(!physics::capsule_cast(world,{0,0,0},1,1,{1,0,0},20),
          "PHYS-005: capsule shorter than diameter rejected");
    auto sphere=world.create("Sphere");
    world.transform(sphere)->local_position={2,0,0};
    auto* ball=world.add_component<physics::SphereCollider>(sphere,physics::sphere_collider_type());
    ball->radius=.25f;ball->layer=5;ball->is_trigger=true;
    auto trigger=physics::sphere_cast(world,{0,0,0},.5f,{1,0,0},20,true,1u<<5);
    check(trigger&&trigger->entity==sphere&&near(trigger->distance,1.25f) &&
          trigger->is_trigger && trigger->layer==5,
          "PHYS-004: sphere target layer-mask and trigger inclusion");
    check(!physics::sphere_cast(world,{0,0,0},.5f,{1,0,0},20,false,1u<<5),
          "PHYS-004: trigger exclusion");
    auto cap=world.create("CapsuleTarget");
    world.transform(cap)->local_position={3,0,0};
    auto* collider=world.add_component<physics::CapsuleCollider>(
        cap,physics::capsule_collider_type());
    collider->radius=.35f;collider->height=2;
    auto cap_hit=physics::capsule_cast(world,{0,0,0},.5f,2,
                                      {1,0,0},20,false);
    check(cap_hit&&cap_hit->entity==cap&&near(cap_hit->distance,2.15f),
          "PHYS-005: capsule-to-capsule conservative contact");
    auto tangent=physics::sphere_cast(world,{3,0,0},.25f,{1,0,0},20,true);
    check(tangent.has_value(),"PHYS-004: initial overlap starts at zero distance");
}
void physics_lifecycle() {
    using namespace nengine;
    core::World world;
    const auto e=world.create("Mover");
    auto* rb=world.add_component<physics::Rigidbody>(e,physics::rigidbody_type());
    rb->linear_velocity={2,0,0};rb->use_gravity=false;
    auto s=physics::step_rigidbodies(world,1);
    check(s.integrated_3d==1&&near(world.transform(e)->local_position.x,2),
          "PHYS-010: dynamic body integrates");
    rb=world.get_component<physics::Rigidbody>(e,physics::rigidbody_type());
    rb->is_kinematic=true;
    s=physics::step_rigidbodies(world,1);
    check(s.integrated_3d==0&&near(world.transform(e)->local_position.x,2),
          "PHYS-010: kinematic body not moved by dynamic integrator");
    rb->is_kinematic=false;rb->enabled=false;
    s=physics::step_rigidbodies(world,1);
    check(s.integrated_3d==0&&near(world.transform(e)->local_position.x,2),
          "PHYS-010: disabled/static body remains stationary");
    rb->enabled=true;
    s=physics::step_rigidbodies(world,.5f);
    check(s.integrated_3d==1&&near(world.transform(e)->local_position.x,3),
          "PHYS-010: dynamic mode resumes without jumps");
    core::World stress;
    for(int i=0;i<1200;++i){
        auto entity=stress.create("Broadphase");
        stress.transform(entity)->local_position={float(i*10),0,0};
        stress.add_component<physics::BoxCollider>(entity,physics::box_collider_type());
    }
    const auto result=physics::detect_box_overlaps(stress);
    check(stress.size()==1200&&result.overlaps.empty()&&
          result.tested_pairs_3d<15000,
          "PHYS-013: 1200 separated colliders culled to sub-quadratic narrowphase pairs");
}
void tilemap_roundtrip() {
    using namespace nengine;
    render::Tilemap map;
    check(map.resize(4,3),"TWOD-001: grid allocated");
    const auto id0=assets::AssetGuid::generate();
    const auto id1=assets::AssetGuid::generate();
    map.palette={id0,id1};map.cell_width=.5f;map.cell_height=2;
    check(map.set(0,0,0)&&map.set(3,2,1)&&!map.set(4,2,0)&&
          !map.set(0,0,3) && map.get(3,2)==1 && map.get(1,1)==-1,
          "TWOD-001: palette bounds and empty cells");
    std::stringstream encoded;
    std::string error;
    check(render::write_tilemap(encoded,map,&error),"TWOD-001: save tilemap");
    render::Tilemap decoded;
    check(render::read_tilemap(encoded,decoded,&error)&&decoded.valid()&&
          decoded.width==4&&decoded.height==3&&
          decoded.palette[0]==id0&&decoded.palette[1]==id1&&
          decoded.get(3,2)==1&&decoded.get(0,0)==0,
          "TWOD-001: serialization roundtrip retains geometry, palette and indices");
    const auto previous=decoded;
    for(auto corrupted:{
       "NENGINE_TILEMAP 999\nGRID 1 1 1 1\nPALETTE 0\nCELLS 1\n-1\nEND_TILEMAP\n",
       "NENGINE_TILEMAP 1\nGRID 1000000 2 1 1\nPALETTE 0\nCELLS 1\n-1\nEND_TILEMAP\n",
       "NENGINE_TILEMAP 1\nGRID 1 1 1 1\nPALETTE 0\nCELLS 1\n44\nEND_TILEMAP\n",
       "NENGINE_TILEMAP 1\nGRID 1 1 1 1\nPALETTE 0\nCELLS 1\n"
    }){
        std::istringstream in{corrupted};
        check(!render::read_tilemap(in,decoded,&error)&&
              decoded.width==previous.width&&
              decoded.get(3,2)==previous.get(3,2),
              "TWOD-001: corrupted tilemap rejected without mutating caller");
    }
    check(map.resize(2,2)&&map.get(0,0)==0&&map.get(1,1)==-1,
          "TWOD-001: resize retains intersection and clears new cells");
}
void particles() {
    using namespace nengine;
    render::ParticleEmitter emitter;
    emitter.rate_per_second=0;emitter.initial_burst=3;
    emitter.gravity={0,-1,0};emitter.lifetime_seconds=.5f;
    emitter.start_size=2;emitter.end_size=0;
    emitter.start_color={1,0,0};emitter.end_color={0,0,1};
    render::ParticleSimulation sim;
    check(sim.update(emitter,{10,2,0},.1f)&&sim.particles().size()==3,
          "VFX-003: initial burst creates exact number of particles");
    check(sim.update(emitter,{10,2,0},.1f)&&sim.particles().size()==3 &&
          sim.particles()[0].position.y>2 &&
          sim.particles()[0].size<2 &&
          sim.particles()[0].color.z>0,
          "VFX-002/003: velocity, gravity, lifetime, size and color interpolated");
    for(int i=0;i<6;++i)sim.update(emitter,{10,2,0},.1f);
    check(sim.particles().empty(),"VFX-002: expired particles retired");
    emitter.rate_per_second=20;emitter.initial_burst=0;
    sim.clear();sim.update(emitter,{0,0,0},.25f);
    check(sim.particles().size()==5,
          "VFX-002: emission rate accumulates deterministic spawn count");
    const auto original=sim.particles().size();
    check(!sim.update(emitter,{},std::numeric_limits<float>::infinity())&&
          sim.particles().size()==original,
          "VFX-002: malformed delta cannot mutate particle state");
    core::ComponentRegistry metadata;
    core::ComponentSerializationRegistry codecs;
    check(render::register_particle_metadata(metadata)&&
          render::register_particle_serializers(codecs),
          "VFX-001: Component metadata and Scene codec registered");
    core::World world;
    const auto owner=world.create("Emitter");
    auto* saved=world.add_component<render::ParticleEmitter>(
        owner,render::particle_emitter_type());
    *saved=emitter;
    core::SceneData data;
    std::string error;
    check(core::SceneSerializer::capture_checked(world,data,"Particles",
         &error,&codecs),"VFX-001: Scene capture includes ParticleEmitter");
    std::stringstream serialized;
    check(core::SceneSerializer::write(data,serialized,&error),
          "VFX-001: Scene serialization of ParticleEmitter");
    core::SceneData parsed;
    check(core::SceneSerializer::read(serialized,parsed,&error),
          "VFX-001: Scene parser accepts ParticleEmitter");
    core::World loaded;
    check(core::SceneSerializer::instantiate(parsed,loaded,&error,&codecs),
          "VFX-001: Scene restore ParticleEmitter");
    if(!loaded.entities().empty()){
        auto* restored=loaded.get_component<render::ParticleEmitter>(
            loaded.entities()[0],render::particle_emitter_type());
        check(restored&&restored->rate_per_second==20 &&
              restored->initial_burst==0 && restored->valid(),
              "VFX-001: emitter settings survive Scene disk roundtrip");
    }
    if(!parsed.objects.empty()&&!parsed.objects[0].components.empty()){
        auto corrupted=parsed;
        for(auto& property:corrupted.objects[0].components[0].properties)
            if(property.name=="Max Particles")property.value=std::int64_t{90000};
        const auto oldsize=loaded.size();
        check(!core::SceneSerializer::instantiate(corrupted,loaded,&error,&codecs) &&
              loaded.size()==oldsize,
              "VFX-001: malformed emitter Scene rejected transactionally");
    }
}
}
int main(){
  continuous_casts();physics_lifecycle();tilemap_roundtrip();particles();
  if(failures)std::cerr<<failures<<" Round Three feature regressions failed\n";
  else std::cout<<"NEngineRoundThreeFeatureTests PASS\n";
  return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
