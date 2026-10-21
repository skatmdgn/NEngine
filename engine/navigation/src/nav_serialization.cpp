#include "nengine/navigation/nav_mesh.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace nengine::navigation {
namespace {
bool fail(std::string* error,const char* msg) {
    if(error)*error=msg;
    return false;
}
} // namespace
bool write_nav_mesh(std::ostream& destination,const NavMesh& mesh,std::string* error){
    // Stage before writing: reject overlarge records without partial output.
    std::ostringstream out;
    out<<std::setprecision(9)<<"NENGINE_NAVMESH 1\nPOLYGONS "
       <<mesh.polygons().size()<<"\n";
    for(const auto& p:mesh.polygons()){
        out<<"POLYGON "<<p.id<<" "<<p.area_mask<<" "<<p.traversal_cost
           <<" "<<(p.enabled?1:0)<<" "<<p.vertices.size()<<"\n";
        for(const auto v:p.vertices)
            out<<"VERTEX "<<v.x<<" "<<v.y<<" "<<v.z<<"\n";
        out<<"END_POLYGON\n";
    }
    out<<"EDGES "<<mesh.edges().size()<<"\n";
    for(const auto& e:mesh.edges())out<<"EDGE "<<e.first<<" "<<e.second<<"\n";
    out<<"LINKS "<<mesh.links().size()<<"\n";
    for(const auto& l:mesh.links())
        out<<"LINK "<<l.id<<" "<<l.from<<" "<<l.to<<" "
           <<l.enter.x<<" "<<l.enter.y<<" "<<l.enter.z<<" "
           <<l.exit.x<<" "<<l.exit.y<<" "<<l.exit.z<<" "
           <<l.cost<<" "<<(l.bidirectional?1:0)<<" "<<(l.enabled?1:0)<<"\n";
    out<<"OBSTACLES "<<mesh.obstacles().size()<<"\n";
    for(const auto& o:mesh.obstacles())
        out<<"OBSTACLE "<<o.id<<" "<<o.center.x<<" "<<o.center.y
           <<" "<<o.center.z<<" "<<o.radius<<" "<<(o.enabled?1:0)<<"\n";
    out<<"END_NAVMESH\n";
    const auto serialized=out.str();
    if(!out.good()||serialized.size()>4u*1024u*1024u)
        return fail(error,"navigation output exceeds 4MiB");
    destination.write(serialized.data(),
                      static_cast<std::streamsize>(serialized.size()));
    if(!destination.good())return fail(error,"navigation stream write failed");
    if(error)error->clear();
    return true;
}
bool read_nav_mesh(std::istream& source,NavMesh& mesh,std::string* error){
    // Read before formatted numeric extraction to avoid unconstrained
    // std::string tokens and keep the old mesh intact on any parse failure.
    constexpr std::size_t maximum=4u*1024u*1024u;
    std::array<char,4096> buffer{};
    std::string text;
    for(;;){
        source.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count=static_cast<std::size_t>(source.gcount());
        if(count>maximum-text.size())
            return fail(error,"navigation input exceeds 4MiB");
        text.append(buffer.data(),count);
        if(source.eof())break;
        if(!source)return fail(error,"navigation input read error");
    }
    std::istringstream in(std::move(text));
    std::string tag;
    int version=0;
    std::size_t count=0;
    if(!(in>>tag>>version)||tag!="NENGINE_NAVMESH"||version!=1||
       !(in>>tag>>count)||tag!="POLYGONS"||count>NavMesh::max_polygons)
        return fail(error,"invalid navigation header");
    NavMesh staged;
    for(std::size_t i=0;i<count;++i){
        NavId id=invalid_nav_id;
        std::uint32_t mask=0;
        float cost=0;
        int enabled=-1;
        std::size_t vertices=0;
        if(!(in>>tag>>id>>mask>>cost>>enabled>>vertices)||
           tag!="POLYGON"||id!=i||vertices<3||
           vertices>NavMesh::max_vertices_per_polygon||
           (enabled!=0&&enabled!=1))
            return fail(error,"invalid navigation polygon");
        std::vector<core::Vec3> polygon;
        polygon.reserve(vertices);
        for(std::size_t j=0;j<vertices;++j){
            core::Vec3 point;
            if(!(in>>tag>>point.x>>point.y>>point.z)||tag!="VERTEX")
                return fail(error,"invalid navigation vertex");
            polygon.push_back(point);
        }
        if(!(in>>tag)||tag!="END_POLYGON"||
           !staged.add_polygon(std::move(polygon),nullptr,mask,cost)||
           !staged.set_polygon_enabled(id,enabled==1))
            return fail(error,"invalid navigation polygon geometry");
    }
    if(!(in>>tag>>count)||tag!="EDGES"||count>16384u)
        return fail(error,"invalid navigation adjacency count");
    for(std::size_t i=0;i<count;++i){
        NavId from=invalid_nav_id,to=invalid_nav_id;
        if(!(in>>tag>>from>>to)||tag!="EDGE"||!staged.connect(from,to))
            return fail(error,"invalid navigation adjacency");
    }
    if(!(in>>tag>>count)||tag!="LINKS"||count>NavMesh::max_links)
        return fail(error,"invalid navigation offmesh count");
    for(std::size_t i=0;i<count;++i){
        NavId id=invalid_nav_id,from=invalid_nav_id,to=invalid_nav_id;
        core::Vec3 enter,exit;
        float cost=0.f;
        int bidirectional=-1,enabled=-1;
        if(!(in>>tag>>id>>from>>to>>enter.x>>enter.y>>enter.z>>
             exit.x>>exit.y>>exit.z>>cost>>bidirectional>>enabled)||
           tag!="LINK"||id!=i||
           (bidirectional!=0&&bidirectional!=1)||
           (enabled!=0&&enabled!=1)||
           !staged.add_link(from,to,enter,exit,cost,bidirectional==1)||
           !staged.set_link_enabled(id,enabled==1))
            return fail(error,"invalid navigation offmesh link");
    }
    if(!(in>>tag>>count)||tag!="OBSTACLES"||count>NavMesh::max_obstacles)
        return fail(error,"invalid navigation obstacle count");
    for(std::size_t i=0;i<count;++i){
        NavId id=invalid_nav_id;
        core::Vec3 center;
        float radius=0.f;
        int enabled=-1;
        if(!(in>>tag>>id>>center.x>>center.y>>center.z>>radius>>enabled)||
           tag!="OBSTACLE"||id!=i||(enabled!=0&&enabled!=1)||
           !staged.add_obstacle(center,radius)||
           !staged.set_obstacle_enabled(id,enabled==1))
            return fail(error,"invalid navigation obstacle");
    }
    if(!(in>>tag)||tag!="END_NAVMESH")
        return fail(error,"invalid navigation terminator");
    in>>std::ws;
    if(!in.eof())return fail(error,"trailing navigation records");
    mesh=std::move(staged);
    if(error)error->clear();
    return true;
}
bool save_nav_mesh(const std::filesystem::path& path,
                   const NavMesh& mesh,std::string* error){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)return fail(error,"cannot open navigation file for writing");
    return write_nav_mesh(out,mesh,error);
}
bool load_nav_mesh(const std::filesystem::path& path,
                   NavMesh& mesh,std::string* error){
    std::error_code ec;
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>4u*1024u*1024u)
        return fail(error,"navigation file missing or exceeds 4MiB");
    std::ifstream input(path,std::ios::binary);
    if(!input)return fail(error,"cannot open navigation file");
    return read_nav_mesh(input,mesh,error);
}
} // namespace nengine::navigation
