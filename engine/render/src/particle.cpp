#include "nengine/render/particle.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <variant>

#include "nengine/core/scene.hpp"
#include "nengine/core/world.hpp"

namespace nengine::render {
namespace {
bool finite(core::Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
bool get_f(const core::SerializedComponentData& d, const char* name, float& out) {
    for (const auto& p:d.properties)
        if(p.name==name) {
            if(auto v=std::get_if<double>(&p.value)) {
                if(!std::isfinite(*v) || *v>std::numeric_limits<float>::max() ||
                   *v < -std::numeric_limits<float>::max())return false;
                out=static_cast<float>(*v); return true;
            }
            return false;
        }
    return false;
}
bool get_v(const core::SerializedComponentData& d, const char* name, core::Vec3& out) {
    for(const auto& p:d.properties)
        if(p.name==name) {
            if(auto v=std::get_if<core::Vec3>(&p.value)){out=*v;return true;}
            return false;
        }
    return false;
}
bool get_i(const core::SerializedComponentData& d, const char* name, std::uint32_t& out) {
    for(const auto& p:d.properties)
        if(p.name==name) {
            if(auto v=std::get_if<std::int64_t>(&p.value);
                v && *v>=0 && *v<=4096){out=static_cast<std::uint32_t>(*v);return true;}
            return false;
        }
    return false;
}
bool get_b(const core::SerializedComponentData& d, const char* name, bool& out) {
    for(const auto& p:d.properties)
        if(p.name==name) {
            if(auto v=std::get_if<bool>(&p.value)){out=*v;return true;}
            return false;
        }
    return false;
}
}
bool ParticleEmitter::valid() const noexcept {
    return std::isfinite(rate_per_second) && rate_per_second>=0 &&
        rate_per_second<=10000 && std::isfinite(lifetime_seconds) &&
        lifetime_seconds>0 && lifetime_seconds<=3600 &&
        std::isfinite(start_size) && start_size>=0 &&
        std::isfinite(end_size) && end_size>=0 &&
        finite(start_color) && finite(end_color) &&
        finite(velocity) && finite(gravity) &&
        initial_burst<=4096 && max_particles>0 && max_particles<=4096;
}
void ParticleSimulation::clear() noexcept {
    particles_.clear(); emission_carry_=0; started_=false;
}
bool ParticleSimulation::update(const ParticleEmitter& e, core::Vec3 origin, float dt) {
    if(!e.valid() || !finite(origin) || !std::isfinite(dt) || dt<0 || dt>1.0f)
        return false;
    if(!e.enabled) return true;
    // Age and integrate existing particles. Semi-implicit Euler remains
    // deterministic for identical dt sequences, independent from wall clock.
    for(auto& p:particles_) {
        p.age+=dt;
        p.velocity.x+=e.gravity.x*dt;
        p.velocity.y+=e.gravity.y*dt;
        p.velocity.z+=e.gravity.z*dt;
        p.position.x+=p.velocity.x*dt;
        p.position.y+=p.velocity.y*dt;
        p.position.z+=p.velocity.z*dt;
        const float t=std::clamp(p.age/p.lifetime,0.0f,1.0f);
        p.size=e.start_size+(e.end_size-e.start_size)*t;
        p.color={
            e.start_color.x+(e.end_color.x-e.start_color.x)*t,
            e.start_color.y+(e.end_color.y-e.start_color.y)*t,
            e.start_color.z+(e.end_color.z-e.start_color.z)*t
        };
    }
    std::erase_if(particles_,[](const Particle& p){return p.age>=p.lifetime;});
    float emitted=e.rate_per_second*dt+emission_carry_;
    const auto count=static_cast<std::uint32_t>(std::floor(emitted));
    emission_carry_=emitted-static_cast<float>(count);
    auto burst=!started_?e.initial_burst:0u;
    started_=true;
    const auto available=static_cast<std::uint32_t>(
        std::max<std::size_t>(0,e.max_particles>particles_.size()
            ? e.max_particles-particles_.size():0));
    const auto add=std::min(available,count+burst);
    for(std::uint32_t i=0;i<add;++i)
        particles_.push_back({origin,e.velocity,e.start_color,
                              e.start_size,0,e.lifetime_seconds});
    return true;
}
bool register_particle_metadata(core::ComponentRegistry& r) {
    bool ok=r.register_type("NEngine.ParticleEmitter","VFX",true,false);
    const auto flags=core::PropertyFlags::Editable|core::PropertyFlags::Serializable;
    for(const auto& [name,kind]:{
            std::pair{"Enabled",core::PropertyKind::Boolean},
            {"Rate",core::PropertyKind::Float},
            {"Lifetime",core::PropertyKind::Float},
            {"Start Size",core::PropertyKind::Float},
            {"End Size",core::PropertyKind::Float},
            {"Start Color",core::PropertyKind::Vec3},
            {"End Color",core::PropertyKind::Vec3},
            {"Velocity",core::PropertyKind::Vec3},
            {"Gravity",core::PropertyKind::Vec3},
            {"Burst",core::PropertyKind::Integer},
            {"Max Particles",core::PropertyKind::Integer}
        }) ok=r.register_property(particle_emitter_type(),{name,kind,flags})&&ok;
    return ok;
}
bool register_particle_serializers(core::ComponentSerializationRegistry& r) {
    using namespace core;
    return r.register_codec({
        particle_emitter_type(),1,"NEngine.ParticleEmitter",
        [](const World& world, Entity entity)->std::optional<SerializedComponentData> {
            const auto* e=world.get_component<ParticleEmitter>(entity,particle_emitter_type());
            if(!e || !e->valid())return std::nullopt;
            SerializedComponentData data;
            const auto b=[&](const char* n,bool x){data.properties.push_back({n,PropertyKind::Boolean,PropertyValue{x}});};
            const auto f=[&](const char* n,float x){data.properties.push_back({n,PropertyKind::Float,PropertyValue{static_cast<double>(x)}});};
            const auto v=[&](const char* n,core::Vec3 x){data.properties.push_back({n,PropertyKind::Vec3,PropertyValue{x}});};
            const auto i=[&](const char* n,std::uint32_t x){data.properties.push_back({n,PropertyKind::Integer,PropertyValue{static_cast<std::int64_t>(x)}});};
            b("Enabled",e->enabled);f("Rate",e->rate_per_second);f("Lifetime",e->lifetime_seconds);
            f("Start Size",e->start_size);f("End Size",e->end_size);
            v("Start Color",e->start_color);v("End Color",e->end_color);
            v("Velocity",e->velocity);v("Gravity",e->gravity);
            i("Burst",e->initial_burst);i("Max Particles",e->max_particles);
            return data;
        },
        [](World& world,Entity entity,const SerializedComponentData& d,std::string* error) {
            ParticleEmitter e;
            if(!get_b(d,"Enabled",e.enabled) ||
               !get_f(d,"Rate",e.rate_per_second) ||
               !get_f(d,"Lifetime",e.lifetime_seconds) ||
               !get_f(d,"Start Size",e.start_size) ||
               !get_f(d,"End Size",e.end_size) ||
               !get_v(d,"Start Color",e.start_color) ||
               !get_v(d,"End Color",e.end_color) ||
               !get_v(d,"Velocity",e.velocity) ||
               !get_v(d,"Gravity",e.gravity) ||
               !get_i(d,"Burst",e.initial_burst) ||
               !get_i(d,"Max Particles",e.max_particles) ||
               !e.valid()) {
                if(error)*error="invalid NEngine.ParticleEmitter component";
                return false;
            }
            auto* existing=world.get_component<ParticleEmitter>(entity,particle_emitter_type());
            if(!existing)existing=world.add_component<ParticleEmitter>(entity,particle_emitter_type());
            if(!existing)return false;
            *existing=e;
            return true;
        }
    });
}
} // namespace nengine::render
