#include "nengine/render/tilemap_physics.hpp"
#include "nengine/physics/components.hpp"
#include <cmath>
#include <utility>

namespace nengine::render {
namespace {
bool fail(std::string* error,const char* msg) {
    if(error)*error=msg;
    return false;
}
constexpr std::size_t max_bodies=16384;
}
bool TilemapCollisionLayer::rebuild(core::World& world,const Tilemap& map,
                                    core::Vec3 origin,std::uint32_t layer,
                                    bool trigger,std::string* error) {
    if(!map.valid() || layer>=32 ||
       !std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z))
        return fail(error,"tilemap physics rejects invalid grid, origin or layer");
    core::World staged=world.clone();
    for(auto e:owned_) {
        if(!staged.is_alive(e) || !staged.destroy(e))
            return fail(error,"tilemap collider ownership changed outside layer");
    }
    std::vector<core::Entity> next;
    for(std::uint32_t row=0;row<map.height;++row) {
        std::uint32_t col=0;
        while(col<map.width) {
            if(map.get(col,row)<0){++col;continue;}
            const auto left=col;
            while(col<map.width && map.get(col,row)>=0)++col;
            if(next.size()>=max_bodies)
                return fail(error,"tilemap exceeds 16384 collision segments");
            auto entity=staged.create("TilemapCollider2D");
            if(!entity.valid())return fail(error,"cannot allocate tilemap collider");
            auto* transform=staged.transform(entity);
            if(!transform)return fail(error,"collider entity missing Transform");
            const auto run=static_cast<float>(col-left);
            transform->local_position={
                origin.x+(static_cast<float>(left)+run*0.5f)*map.cell_width,
                origin.y+(static_cast<float>(row)+0.5f)*map.cell_height,
                origin.z
            };
            auto* collider=staged.add_component<physics::BoxCollider2D>(
                entity,physics::box_collider2d_type());
            if(!collider)return fail(error,"cannot attach static BoxCollider2D");
            collider->size={run*map.cell_width,map.cell_height,0};
            collider->layer=layer;
            collider->is_trigger=trigger;
            next.push_back(entity);
        }
    }
    world=std::move(staged);
    owned_=std::move(next);
    if(error)error->clear();
    return true;
}
bool TilemapCollisionLayer::clear(core::World& world,std::string* error) {
    for(auto e:owned_)
        if(!world.is_alive(e))
            return fail(error,"tilemap collider entity externally removed");
    core::World staged=world.clone();
    for(auto e:owned_)if(!staged.destroy(e))
        return fail(error,"tilemap collider removal failed");
    world=std::move(staged);
    owned_.clear();
    if(error)error->clear();
    return true;
}
} // namespace nengine::render
