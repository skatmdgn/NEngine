#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "nengine/core/prefab.hpp"

namespace nengine::editor {

// EDTOOL-002: changes happen in a detached World until Save is explicitly
// accepted. This never edits the source Scene or a live PrefabInstance.
class PrefabIsolationSession {
public:
    bool open(const core::PrefabData& prefab,
              const core::ComponentSerializationRegistry& codecs,
              std::string* error=nullptr) {
        if(active_){
            if(error)*error="finish or cancel current prefab isolation first";
            return false;
        }
        if(!core::PrefabModel::validate(prefab,error))return false;
        auto root_it=std::find_if(prefab.template_scene.objects.begin(),
            prefab.template_scene.objects.end(),
            [&](const auto& item){return item.local_id==prefab.root_local_id;});
        if(root_it==prefab.template_scene.objects.end())return false;
        core::World candidate;
        if(!core::SceneSerializer::instantiate(
               prefab.template_scene,candidate,error,&codecs))return false;
        const auto index=static_cast<std::size_t>(
            std::distance(prefab.template_scene.objects.begin(),root_it));
        const auto entities=candidate.entities();
        if(index>=entities.size())return false;
        editable_=std::move(candidate);
        original_=prefab;
        root_=entities[index];
        codecs_=&codecs;
        active_=true;
        if(error)error->clear();
        return true;
    }
    bool active() const noexcept {return active_;}
    core::World* world() noexcept {return active_?&editable_:nullptr;}
    const core::World* world() const noexcept {return active_?&editable_:nullptr;}
    void cancel() noexcept {
        active_=false;
        editable_=core::World{};
        original_={};
        root_=core::Entity::invalid();
        codecs_=nullptr;
    }
    bool save(core::PrefabData& output,std::string* error=nullptr) const {
        if(!active_||!codecs_||!editable_.is_alive(root_)){
            if(error)*error="isolated prefab root is missing";
            return false;
        }
        core::SceneData scene;
        if(!core::SceneSerializer::capture_checked(
            editable_,scene,original_.name,error,codecs_))return false;
        const auto entities=editable_.entities();
        auto pos=std::find(entities.begin(),entities.end(),root_);
        if(pos==entities.end())return false;
        auto next=original_;
        next.template_scene=std::move(scene);
        next.root_local_id=static_cast<std::uint64_t>(
            std::distance(entities.begin(),pos));
        if(!core::PrefabModel::validate(next,error))return false;
        output=std::move(next);
        if(error)error->clear();
        return true;
    }
private:
    bool active_{false};
    core::World editable_{};
    core::PrefabData original_{};
    core::Entity root_{core::Entity::invalid()};
    const core::ComponentSerializationRegistry* codecs_{nullptr};
};

} // namespace nengine::editor
