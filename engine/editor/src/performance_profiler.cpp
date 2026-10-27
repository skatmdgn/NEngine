#include "nengine/editor/performance_profiler.hpp"
#include <algorithm>
#include <cmath>
namespace nengine::editor {
void PerformanceProfiler::sample(ProfileFrame f) noexcept {
    if(!std::isfinite(f.frame_ms)||!std::isfinite(f.simulation_ms)||
       f.frame_ms<0||f.simulation_ms<0||f.frame_ms>60000||
       f.simulation_ms>60000)return;
    if(samples_.size()>=capacity)samples_.pop_front();
    samples_.push_back(f);
}
ProfileSummary PerformanceProfiler::summary() const noexcept {
    ProfileSummary out;
    out.samples=samples_.size();
    if(samples_.empty())return out;
    for(const auto& f:samples_) {
        out.average_ms+=f.frame_ms;
        out.average_simulation_ms+=f.simulation_ms;
        out.peak_ms=std::max(out.peak_ms,f.frame_ms);
    }
    out.average_ms/=static_cast<double>(samples_.size());
    out.average_simulation_ms/=static_cast<double>(samples_.size());
    out.entity_count=samples_.back().entity_count;
    out.playing=samples_.back().playing;
    return out;
}
}
