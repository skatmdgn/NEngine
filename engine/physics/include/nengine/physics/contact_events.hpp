#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "nengine/physics/collision.hpp"

namespace nengine::physics {

enum class ContactPhase : std::uint8_t {
    Enter,
    Stay,
    Exit
};

struct ContactEvent {
    core::Entity first{
        core::Entity::invalid()};
    core::Entity second{
        core::Entity::invalid()};
    ContactPhase phase{
        ContactPhase::Enter};
    bool is_trigger{false};
    bool is_2d{false};
    core::Vec3 normal{};
    float penetration{0.0f};
    core::Vec3 point{};
    std::size_t contact_count{0};
};

class ContactTracker {
public:
    std::vector<ContactEvent> update(
        const std::vector<BoxOverlap>& overlaps);

    void clear() noexcept;

    std::size_t active_pair_count() const noexcept {
        return previous_.size();
    }

private:
    struct PairKey {
        std::uint64_t first{0};
        std::uint64_t second{0};
        bool is_trigger{false};
        bool is_2d{false};

        friend bool operator==(
            const PairKey&,
            const PairKey&) = default;
    };

    struct PairKeyHash {
        std::size_t operator()(
            const PairKey& key) const noexcept;
    };

    static PairKey make_key(
        const BoxOverlap& overlap) noexcept;

    std::unordered_map<
        PairKey,
        BoxOverlap,
        PairKeyHash>
        previous_{};
};

} // namespace nengine::physics
