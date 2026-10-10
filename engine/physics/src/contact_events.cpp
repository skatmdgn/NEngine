#include "nengine/physics/contact_events.hpp"

#include <algorithm>
#include <functional>
#include <utility>

namespace nengine::physics {

std::size_t ContactTracker::PairKeyHash::operator()(
    const PairKey& key) const noexcept {

    std::size_t seed =
        std::hash<std::uint64_t>{}(
            key.first);

    const auto combine =
        [&seed](std::size_t value) {
            seed ^=
                value +
                0x9e3779b9u +
                (seed << 6u) +
                (seed >> 2u);
        };

    combine(
        std::hash<std::uint64_t>{}(
            key.second));

    combine(
        std::hash<bool>{}(
            key.is_trigger));

    combine(
        std::hash<bool>{}(
            key.is_2d));

    return seed;
}

ContactTracker::PairKey ContactTracker::make_key(
    const BoxOverlap& overlap) noexcept {

    const auto a =
        overlap.first.value;
    const auto b =
        overlap.second.value;

    return {
        std::min(a, b),
        std::max(a, b),
        overlap.is_trigger,
        overlap.is_2d
    };
}

std::vector<ContactEvent> ContactTracker::update(
    const std::vector<BoxOverlap>& overlaps) {

    std::unordered_map<
        PairKey,
        BoxOverlap,
        PairKeyHash>
        current;

    current.reserve(
        overlaps.size());

    for (const auto& overlap :
         overlaps) {

        if (!overlap.first.valid() ||
            !overlap.second.valid()) {
            continue;
        }

        current.insert_or_assign(
            make_key(overlap),
            overlap);
    }

    std::vector<ContactEvent> events;
    events.reserve(
        current.size() +
        previous_.size());

    for (const auto& [key, overlap] :
         current) {

        const bool existed =
            previous_.find(key) !=
            previous_.end();

        const core::Vec3 point =
            overlap.manifold.count > 0u
                ? overlap.manifold
                    .points[0]
                    .point
                : core::Vec3{};

        events.push_back({
            overlap.first,
            overlap.second,
            existed
                ? ContactPhase::Stay
                : ContactPhase::Enter,
            overlap.is_trigger,
            overlap.is_2d,
            overlap.normal,
            overlap.penetration,
            point,
            overlap.manifold.count
        });
    }

    for (const auto& [key, overlap] :
         previous_) {

        if (current.find(key) !=
            current.end()) {
            continue;
        }

        const core::Vec3 point =
            overlap.manifold.count > 0u
                ? overlap.manifold
                    .points[0]
                    .point
                : core::Vec3{};

        events.push_back({
            overlap.first,
            overlap.second,
            ContactPhase::Exit,
            overlap.is_trigger,
            overlap.is_2d,
            overlap.normal,
            0.0f,
            point,
            0u
        });
    }

    previous_ =
        std::move(current);

    return events;
}

void ContactTracker::clear() noexcept {
    previous_.clear();
}

} // namespace nengine::physics
