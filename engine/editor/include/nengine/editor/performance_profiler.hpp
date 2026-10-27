#pragma once
#include <cstddef>
#include <deque>
namespace nengine::editor {
struct ProfileFrame {
    double frame_ms{0};
    double simulation_ms{0};
    std::size_t entity_count{0};
    bool playing{false};
};
struct ProfileSummary {
    std::size_t samples{0};
    double average_ms{0};
    double peak_ms{0};
    double average_simulation_ms{0};
    std::size_t entity_count{0};
    bool playing{false};
};
// EDTOOL-006: bounded Editor / in-Editor Game simulation profile, never
// a false claim of GPU timer queries or an installed standalone Player.
class PerformanceProfiler {
public:
    void sample(ProfileFrame frame) noexcept;
    ProfileSummary summary() const noexcept;
    void clear() noexcept {samples_.clear();}
    std::size_t size() const noexcept{return samples_.size();}
private:
    std::deque<ProfileFrame> samples_{};
    static constexpr std::size_t capacity=240;
};
}
