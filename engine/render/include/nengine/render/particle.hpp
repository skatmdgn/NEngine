#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/math.hpp"

namespace nengine::render {

struct ParticleEmitter {
    bool enabled{true};
    float rate_per_second{10.0f};
    float lifetime_seconds{1.0f};
    float start_size{1.0f};
    float end_size{0.0f};
    core::Vec3 start_color{1,1,1};
    core::Vec3 end_color{1,1,1};
    core::Vec3 velocity{0,1,0};
    core::Vec3 gravity{0,-9.81f,0};
    std::uint32_t initial_burst{0};
    std::uint32_t max_particles{512};
    bool valid() const noexcept;
};
inline core::ComponentTypeId particle_emitter_type() noexcept {
    return core::ComponentRegistry::stable_id("NEngine.ParticleEmitter");
}
struct Particle {
    core::Vec3 position{};
    core::Vec3 velocity{};
    core::Vec3 color{};
    float size{1.0f};
    float age{0.0f};
    float lifetime{1.0f};
};

class ParticleSimulation {
public:
    // CPU-only deterministic state; rendering is a separate later task.
    // Simulation never changes on invalid emitter config or nonfinite dt.
    bool update(const ParticleEmitter& emitter, core::Vec3 origin, float dt);
    void clear() noexcept;
    const std::vector<Particle>& particles() const noexcept { return particles_; }
private:
    std::vector<Particle> particles_{};
    float emission_carry_{0};
    bool started_{false};
};

bool register_particle_metadata(core::ComponentRegistry& registry);
bool register_particle_serializers(core::ComponentSerializationRegistry& registry);
} // namespace nengine::render
