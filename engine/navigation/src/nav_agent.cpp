#include "nengine/navigation/nav_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace nengine::navigation {
namespace {
float separation(core::Vec3 a,core::Vec3 b) noexcept {
    const float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}
} // namespace
bool NavAgent::assign(const NavPath& path) noexcept {
    if(path.waypoints.empty()||!finite_position(position)||
       !std::isfinite(speed)||speed<0.f||
       !std::isfinite(stopping_distance)||stopping_distance<0.f)
        return false;
    for(const auto point:path.waypoints)
        if(!finite_position(point))return false;
    try {
        auto copy=path.waypoints;
        route=std::move(copy);
    } catch (...) {
        return false;
    }
    next_waypoint=0;
    velocity={};
    arrived=false;
    return true;
}
void NavAgent::stop() noexcept {
    route.clear();next_waypoint=0;velocity={};arrived=true;
}
bool NavAgent::update(float dt) noexcept {
    if(!enabled||!std::isfinite(dt)||dt<0.f||dt>10.f||
       !std::isfinite(speed)||speed<0.f||
       !std::isfinite(stopping_distance)||stopping_distance<0.f||
       !finite_position(position))return false;
    if(arrived||route.empty()) {
        velocity={};
        return true;
    }
    const float previous_dt=dt;
    const auto start=position;
    float remaining=dt*speed;
    // Bound the loop even for zero-length repeated waypoints.
    for(std::size_t iterations=0;iterations<=route.size();++iterations){
        if(next_waypoint>=route.size()){
            stop();
            break;
        }
        const auto target=route[next_waypoint];
        const float dist=separation(position,target);
        const bool last=next_waypoint+1==route.size();
        if(last&&dist<=stopping_distance){
            stop();
            break;
        }
        if(dist<=0.0001f){
            ++next_waypoint;
            continue;
        }
        if(remaining<=0.f)break;
        const float travel=std::min(remaining,
            last?std::max(0.f,dist-stopping_distance):dist);
        const float ratio=travel/dist;
        position.x+=(target.x-position.x)*ratio;
        position.y+=(target.y-position.y)*ratio;
        position.z+=(target.z-position.z)*ratio;
        remaining-=travel;
        if(!finite_position(position)) {
            position=start;velocity={};return false;
        }
        if(travel>=dist-0.0001f||last&&dist-travel<=stopping_distance+0.0001f){
            if(last) {
                stop();
                break;
            }
            ++next_waypoint;
        } else break;
    }
    if(previous_dt>0.f && !arrived)
        velocity={(position.x-start.x)/previous_dt,
                  (position.y-start.y)/previous_dt,
                  (position.z-start.z)/previous_dt};
    else velocity={};
    return true;
}
} // namespace nengine::navigation
