#include "nengine/navigation/nav_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace nengine::navigation {
namespace {
constexpr float geometry_epsilon = 0.0001f;
float cross_xz(core::Vec3 a, core::Vec3 b, core::Vec3 c) noexcept {
    return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
}
float sq_xz(core::Vec3 a, core::Vec3 b) noexcept {
    return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);
}
bool same_xz(core::Vec3 a, core::Vec3 b) noexcept {
    return sq_xz(a,b)<geometry_epsilon*geometry_epsilon &&
           std::abs(a.y-b.y)<=0.25f;
}
float edge_sq(core::Vec3 point, core::Vec3 a, core::Vec3 b) noexcept {
    const float dx=b.x-a.x, dz=b.z-a.z;
    const float sq=dx*dx+dz*dz;
    if (sq<=geometry_epsilon*geometry_epsilon) return sq_xz(point,a);
    const float t=std::clamp(((point.x-a.x)*dx+(point.z-a.z)*dz)/sq,0.f,1.f);
    const core::Vec3 nearest{a.x+t*dx,a.y+t*(b.y-a.y),a.z+t*dz};
    return sq_xz(point,nearest);
}
bool point_in_polygon(const NavPolygon& poly, core::Vec3 point) noexcept {
    if (!finite_position(point) || std::abs(point.y-poly.center.y)>2.f)
        return false;
    bool positive=false,negative=false;
    for (std::size_t i=0;i<poly.vertices.size();++i) {
        const float cross=cross_xz(poly.vertices[i],
            poly.vertices[(i+1)%poly.vertices.size()],point);
        if(cross>geometry_epsilon) positive=true;
        if(cross< -geometry_epsilon) negative=true;
        if(positive && negative) return false;
    }
    return true;
}
bool shared_edge(const NavPolygon& a,const NavPolygon& b) noexcept {
    for (std::size_t i=0;i<a.vertices.size();++i)
        for (std::size_t j=0;j<b.vertices.size();++j)
            if(same_xz(a.vertices[i],b.vertices[(j+1)%b.vertices.size()]) &&
               same_xz(a.vertices[(i+1)%a.vertices.size()],b.vertices[j]))
                return true;
    return false;
}
} // namespace

bool finite_position(core::Vec3 p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z) &&
           std::abs(p.x)<=1000000.f && std::abs(p.y)<=1000000.f &&
           std::abs(p.z)<=1000000.f;
}
bool NavMesh::add_polygon(std::vector<core::Vec3> vertices,
                         NavId* out_id,std::uint32_t area,float cost) {
    if(polygons_.size()>=max_polygons || vertices.size()<3 ||
       vertices.size()>max_vertices_per_polygon || area==0 ||
       !std::isfinite(cost)||cost<1.f || cost>100000.f)
        return false;
    core::Vec3 center{};
    const float height=vertices[0].y;
    for (const auto p:vertices) {
        if(!finite_position(p)||std::abs(height-p.y)>0.25f) return false;
        center.x+=p.x/static_cast<float>(vertices.size());
        center.y+=p.y/static_cast<float>(vertices.size());
        center.z+=p.z/static_cast<float>(vertices.size());
    }
    // Convex polygons only. Consistent winding, nonzero edges and no
    // collinear/repeated corners prevent ambiguous point-in-polygon tests.
    int sign=0;
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto a=vertices[i],b=vertices[(i+1)%vertices.size()],
                   c=vertices[(i+2)%vertices.size()];
        if(sq_xz(a,b)<=geometry_epsilon*geometry_epsilon) return false;
        const float turn=cross_xz(a,b,c);
        if(!std::isfinite(turn)||std::abs(turn)<=geometry_epsilon)
            return false;
        const int current=turn>0?1:-1;
        if(sign && sign!=current) return false;
        sign=current;
    }
    const NavId id=static_cast<NavId>(polygons_.size());
    polygons_.push_back({id,std::move(vertices),center,area,cost,true});
    if(out_id) *out_id=id;
    return true;
}
const NavPolygon* NavMesh::polygon(NavId id) const noexcept {
    return id<polygons_.size()?&polygons_[id]:nullptr;
}
bool NavMesh::set_polygon_enabled(NavId id,bool enabled) noexcept {
    if(id>=polygons_.size())return false;
    polygons_[id].enabled=enabled;
    return true;
}
bool NavMesh::set_polygon_area(NavId id,std::uint32_t mask,float cost) noexcept {
    if(id>=polygons_.size()||mask==0||!std::isfinite(cost)||
       cost<1.f||cost>100000.f)return false;
    polygons_[id].area_mask=mask;
    polygons_[id].traversal_cost=cost;
    return true;
}
bool NavMesh::contains(NavId id,core::Vec3 point) const noexcept {
    const auto* poly=polygon(id);
    return poly&&point_in_polygon(*poly,point);
}
bool NavMesh::connect(NavId a,NavId b) {
    if(a==b||a>=polygons_.size()||b>=polygons_.size()||
       edges_.size()>=16384u||!shared_edge(polygons_[a],polygons_[b]))return false;
    const auto pair=std::minmax(a,b);
    for(const auto& edge:edges_)
        if(edge.first==pair.first&&edge.second==pair.second)return false;
    edges_.push_back({pair.first,pair.second});
    return true;
}
bool NavMesh::add_link(NavId from,NavId to,core::Vec3 enter,
                       core::Vec3 exit,float cost,bool bidirectional,NavId* out_id){
    if(links_.size()>=max_links||from==to||from>=polygons_.size()||
       to>=polygons_.size()||!contains(from,enter)||!contains(to,exit)||
       !std::isfinite(cost)||cost<1.f||cost>100000.f)return false;
    const NavId id=static_cast<NavId>(links_.size());
    links_.push_back({id,from,to,enter,exit,cost,bidirectional,true});
    if(out_id)*out_id=id;
    return true;
}
bool NavMesh::set_link_enabled(NavId id,bool enabled) noexcept {
    if(id>=links_.size())return false;
    links_[id].enabled=enabled;
    return true;
}
bool NavMesh::add_obstacle(core::Vec3 center,float radius,NavId* out_id){
    if(obstacles_.size()>=max_obstacles||!finite_position(center)||
       !std::isfinite(radius)||radius<=0.f||radius>100000.f)return false;
    const NavId id=static_cast<NavId>(obstacles_.size());
    obstacles_.push_back({id,center,radius,true});
    if(out_id)*out_id=id;
    return true;
}
bool NavMesh::remove_obstacle(NavId id) noexcept {
    if(id>=obstacles_.size()||!obstacles_[id].enabled)return false;
    obstacles_[id].enabled=false;
    return true;
}
bool NavMesh::set_obstacle_enabled(NavId id,bool enabled) noexcept {
    if(id>=obstacles_.size())return false;
    obstacles_[id].enabled=enabled;
    return true;
}
bool NavMesh::blocked(NavId id,float agent_radius) const noexcept {
    if(id>=polygons_.size()||!std::isfinite(agent_radius)||
       agent_radius<0.f||agent_radius>100000.f)return true;
    const auto& poly=polygons_[id];
    for(const auto& obs:obstacles_){
        if(!obs.enabled||std::abs(obs.center.y-poly.center.y)>2.f)continue;
        const float radius=obs.radius+agent_radius;
        if(point_in_polygon(poly,obs.center))return true;
        for(std::size_t j=0;j<poly.vertices.size();++j)
            if(edge_sq(obs.center,poly.vertices[j],
                 poly.vertices[(j+1)%poly.vertices.size()])<=radius*radius)
                return true;
    }
    return false;
}
std::optional<NavId> NavMesh::locate(core::Vec3 p,const NavQuery& q) const noexcept {
    if(!finite_position(p)||q.area_mask==0||!std::isfinite(q.agent_radius)||
       q.agent_radius<0.f)return std::nullopt;
    for(const auto& poly:polygons_)
        if(poly.enabled&&(poly.area_mask&q.area_mask)!=0u &&
           !blocked(poly.id,q.agent_radius)&&point_in_polygon(poly,p))
            return poly.id;
    return std::nullopt;
}
void NavMesh::clear() noexcept {
    polygons_.clear();edges_.clear();links_.clear();obstacles_.clear();
}
} // namespace nengine::navigation
