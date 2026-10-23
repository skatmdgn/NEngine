#include "nengine/core/additive_scene.hpp"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace nengine::core {
namespace {
bool fail(std::string* error,const char* why){
    if(error)*error=why;
    return false;
}
}

bool AdditiveScenes::owns(AdditiveHandle handle,Entity entity) const noexcept {
    for(const auto& instance:instances_)if(instance.handle==handle)
        for(auto owned:instance.entities)if(owned==entity)return true;
    return false;
}

bool AdditiveScenes::load(
    const SceneData& scene, World& destination,AdditiveHandle& result,
    std::string* error,const ComponentSerializationRegistry* codecs) {
    if(scene.version>SceneData::current_version || scene.objects.size()>100000 ||
       next_==std::numeric_limits<std::uint64_t>::max())
        return fail(error,"invalid additive scene version, size or handle exhausted");
    World staged=destination.clone();
    std::unordered_map<std::uint64_t,Entity> mapping;
    std::vector<Entity> created;
    mapping.reserve(scene.objects.size());
    created.reserve(scene.objects.size());
    for(const auto& object:scene.objects) {
        if(mapping.contains(object.local_id))
            return fail(error,"additive scene duplicate local ID");
        auto entity=staged.create(object.name);
        if(!entity.valid())return fail(error,"cannot allocate additive entity");
        auto* transform=staged.transform(entity);
        if(!transform)return fail(error,"additive entity missing Transform");
        *transform=object.transform;
        transform->parent=Entity::invalid();
        staged.set_active(entity,object.active);
        mapping.emplace(object.local_id,entity);
        created.push_back(entity);
    }
    for(const auto& object:scene.objects) {
        if(object.parent_local_id<0)continue;
        const auto parent=mapping.find(static_cast<std::uint64_t>(object.parent_local_id));
        if(parent==mapping.end()||
           !staged.set_parent(mapping.at(object.local_id),parent->second))
            return fail(error,"invalid/cyclic additive hierarchy");
    }
    for(const auto& object:scene.objects) {
        if(object.components.empty())continue;
        if(!codecs)return fail(error,"missing additive Scene component codecs");
        for(const auto& original:object.components) {
            auto part=original;
            for(auto& prop:part.properties){
                if(prop.kind!=PropertyKind::EntityReference)continue;
                if(auto local=std::get_if<std::uint64_t>(&prop.value)){
                    auto target=mapping.find(*local);
                    if(target==mapping.end())
                        return fail(error,"missing additive scene-local EntityReference");
                    prop.value=target->second;
                } else if(auto nullvalue=std::get_if<std::int64_t>(&prop.value);
                          nullvalue&&*nullvalue<0){
                    prop.value=Entity::invalid();
                } else return fail(error,"invalid additive EntityReference");
            }
            if(!codecs->restore(staged,mapping.at(object.local_id),part,error))
                return false;
        }
    }
    // Commit only after every entity, parent and component succeeds.
    destination=std::move(staged);
    const auto token=AdditiveHandle{next_++};
    instances_.push_back({token,std::move(created)});
    result=token;
    if(error)error->clear();
    return true;
}

bool AdditiveScenes::unload(
    AdditiveHandle handle,World& destination,std::string* error,
    const ComponentSerializationRegistry* codecs) {
    auto it=std::find_if(instances_.begin(),instances_.end(),
        [&](const Instance& x){return x.handle==handle;});
    if(it==instances_.end())
        return fail(error,"unknown additive scene handle");
    World staged=destination.clone();
    std::unordered_set<Entity::value_type> removed;
    for(auto e:it->entities) {
        if(!staged.is_alive(e))
            return fail(error,"additive scene ownership invalidated by external deletion");
        removed.insert(e.value);
    }
    // If a remaining component stores a reference to an unloaded Entity,
    // null it using the registered codec. Reject unknown component types
    // rather than leaving a silently dangling reference.
    for(auto owner:staged.entities()){
        if(removed.contains(owner.value))continue;
        for(auto type:staged.component_types(owner)){
            if(type==World::transform_type)continue;
            if(!codecs || !codecs->find(type))
                return fail(error,"cannot inspect cross-scene component references");
            auto data=codecs->capture(staged,owner,type);
            if(!data)
                return fail(error,"cannot capture component before additive unload");
            bool changed=false;
            for(auto& property:data->properties) {
                if(property.kind!=PropertyKind::EntityReference)continue;
                if(auto target=std::get_if<Entity>(&property.value);
                   target && removed.contains(target->value)){
                    property.value=Entity::invalid();
                    changed=true;
                }
            }
            if(changed&&!codecs->restore(staged,owner,*data,error))
                return false;
        }
    }
    for(auto e:it->entities)
        if(!staged.destroy(e))
            return fail(error,"additive scene unload failed");
    destination=std::move(staged);
    instances_.erase(it);
    if(error)error->clear();
    return true;
}
} // namespace nengine::core
