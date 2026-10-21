#include "nengine/navigation/nav_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <tuple>
#include <vector>

namespace nengine::navigation {
namespace {
float distance(core::Vec3 a,core::Vec3 b) noexcept {
    const double dx=static_cast<double>(a.x)-b.x;
    const double dy=static_cast<double>(a.y)-b.y;
    const double dz=static_cast<double>(a.z)-b.z;
    return static_cast<float>(std::sqrt(dx*dx+dy*dy+dz*dz));
}
bool same(core::Vec3 a,core::Vec3 b) noexcept {
    return distance(a,b)<0.0001f;
}
void append_waypoint(std::vector<core::Vec3>& path,core::Vec3 p) {
    if(path.empty()||!same(path.back(),p))path.push_back(p);
}
struct SearchStep {
    float cost{std::numeric_limits<float>::infinity()};
    NavId predecessor{invalid_nav_id};
    NavId link{invalid_nav_id};
    bool reverse{false};
    bool settled{false};
};
} // namespace

std::optional<NavPath> NavMesh::find_path(
    core::Vec3 start,core::Vec3 goal,const NavQuery& query) const {
    if(!finite_position(start)||!finite_position(goal)||
       query.area_mask==0||!std::isfinite(query.agent_radius)||
       query.agent_radius<0.f||query.agent_radius>100000.f||
       query.max_expansions==0u)return std::nullopt;
    const auto start_polygon=locate(start,query);
    const auto goal_polygon=locate(goal,query);
    if(!start_polygon||!goal_polygon)return std::nullopt;
    NavPath path;
    if(*start_polygon==*goal_polygon){
        path.polygons={*start_polygon};
        append_waypoint(path.waypoints,start);
        append_waypoint(path.waypoints,goal);
        path.expansions=1u;
        path.cost=distance(start,goal)*polygons_[*start_polygon].traversal_cost;
        return path;
    }
    // Dijkstra rather than A*: cheap off-mesh links can invalidate a
    // Euclidean heuristic, so deterministic shortest paths come first.
    std::vector<SearchStep> nodes(polygons_.size());
    using Entry=std::pair<float,NavId>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<>> frontier;
    nodes[*start_polygon].cost=0.f;
    frontier.push({0.f,*start_polygon});
    NavId closest=*start_polygon;
    float closest_metric=distance(polygons_[closest].center,goal);
    const std::size_t budget=std::min(query.max_expansions,max_polygons);
    auto permitted=[&](NavId id){
        return id<polygons_.size()&&polygons_[id].enabled&&
               (polygons_[id].area_mask&query.area_mask)!=0 &&
               !blocked(id,query.agent_radius);
    };
    while(!frontier.empty()&&path.expansions<budget){
        const auto [score,id]=frontier.top();frontier.pop();
        auto& node=nodes[id];
        if(node.settled||score>node.cost)continue;
        node.settled=true;
        ++path.expansions;
        const float metric=distance(polygons_[id].center,goal);
        if(metric<closest_metric-0.00001f ||
           (std::abs(metric-closest_metric)<=0.00001f&&id<closest)){
            closest=id;
            closest_metric=metric;
        }
        if(id==*goal_polygon)break;
        const auto relax=[&](NavId next,float edge_cost,NavId link,bool reverse){
            if(!permitted(next)||nodes[next].settled||
               !std::isfinite(edge_cost)||edge_cost<0.f)return;
            const float combined=node.cost+edge_cost*polygons_[next].traversal_cost;
            if(!std::isfinite(combined)||combined>=nodes[next].cost)return;
            nodes[next].cost=combined;
            nodes[next].predecessor=id;
            nodes[next].link=link;
            nodes[next].reverse=reverse;
            frontier.push({combined,next});
        };
        for(const auto& edge:edges_){
            if(edge.first==id||edge.second==id){
                const auto next=edge.first==id?edge.second:edge.first;
                relax(next,distance(polygons_[id].center,polygons_[next].center),
                      invalid_nav_id,false);
            }
        }
        for(const auto& link:links_){
            if(!link.enabled)continue;
            if(link.from==id){
                const float cost=distance(polygons_[id].center,link.enter)+link.cost+
                                 distance(link.exit,polygons_[link.to].center);
                relax(link.to,cost,link.id,false);
            }
            if(link.bidirectional&&link.to==id){
                const float cost=distance(polygons_[id].center,link.exit)+link.cost+
                                 distance(link.enter,polygons_[link.from].center);
                relax(link.from,cost,link.id,true);
            }
        }
    }
    const bool reached=nodes[*goal_polygon].settled;
    const NavId destination=reached?*goal_polygon:closest;
    if(!reached&&(!query.allow_partial||destination==*start_polygon))
        return std::nullopt;
    path.status=reached?NavPathStatus::Complete:NavPathStatus::Partial;
    path.cost=nodes[destination].cost;
    std::vector<NavId> backward;
    NavId cursor=destination;
    while(cursor!=invalid_nav_id&&backward.size()<=polygons_.size()){
        backward.push_back(cursor);
        if(cursor==*start_polygon)break;
        cursor=nodes[cursor].predecessor;
    }
    if(backward.empty()||backward.back()!=*start_polygon)return std::nullopt;
    path.polygons.assign(backward.rbegin(),backward.rend());
    append_waypoint(path.waypoints,start);
    for(std::size_t i=1;i<path.polygons.size();++i){
        const auto to=path.polygons[i];
        const auto& step=nodes[to];
        if(step.link==invalid_nav_id){
            if(i+1<path.polygons.size())
                append_waypoint(path.waypoints,polygons_[to].center);
            continue;
        }
        const auto& link=links_[step.link];
        append_waypoint(path.waypoints,step.reverse?link.exit:link.enter);
        append_waypoint(path.waypoints,step.reverse?link.enter:link.exit);
        path.used_offmesh_link=true;
    }
    append_waypoint(path.waypoints,
        reached?goal:polygons_[destination].center);
    return path;
}
} // namespace nengine::navigation
