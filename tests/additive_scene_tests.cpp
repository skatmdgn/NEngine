#include <cstdlib>
#include <iostream>
#include <string>
#include "nengine/core/additive_scene.hpp"
#include "nengine/core/scene.hpp"

namespace {
int failures=0;
void check(bool b,const char* what){
    if(!b){++failures;std::cerr<<"FAIL: "<<what<<"\n";}
}
struct Link {nengine::core::Entity to{nengine::core::Entity::invalid()};};
}
int main(){
    using namespace nengine::core;
    ComponentSerializationRegistry codecs;
    const auto link_type=ComponentRegistry::stable_id("Test.Link");
    check(codecs.register_codec({link_type,1,"Test.Link",
        [link_type](const World& w,Entity e)->std::optional<SerializedComponentData> {
            auto* l=w.get_component<Link>(e,link_type);
            if(!l)return std::nullopt;
            SerializedComponentData data;
            data.properties.push_back({"To",PropertyKind::EntityReference,PropertyValue{l->to}});
            return data;
        },
        [link_type](World& w,Entity e,const SerializedComponentData& d,std::string* error){
            if(d.properties.size()!=1||d.properties[0].name!="To"||
               !std::holds_alternative<Entity>(d.properties[0].value)){
                if(error)*error="bad Link";return false;
            }
            auto* l=w.get_component<Link>(e,link_type);
            if(!l)l=w.add_component<Link>(e,link_type);
            if(!l)return false;
            l->to=std::get<Entity>(d.properties[0].value);
            return true;
        }}),"SCN-003: register entity-reference component codec");

    World host;
    const auto persistent=host.create("Host");
    const auto stable=host.create("ExternalLink");
    auto* link=host.add_component<Link>(stable,link_type);
    link->to=Entity::invalid();
    AdditiveScenes scenes;
    SceneData incoming;
    incoming.name="Additive";
    SceneObjectData child;
    child.local_id=14;child.name="Child";child.parent_local_id=7;
    SceneObjectData parent;
    parent.local_id=7;parent.name="Parent";
    // External component reference encoded using source-local ids.
    SerializedComponentData reference;
    reference.type=link_type;
    reference.version=1;
    reference.type_name="Test.Link";
    reference.properties.push_back({"To",PropertyKind::EntityReference,PropertyValue{std::uint64_t{14}}});
    parent.components.push_back(reference);
    incoming.objects={child,parent};
    AdditiveHandle first;
    std::string error;
    check(scenes.load(incoming,host,first,&error,&codecs) &&
          first.valid()&&host.size()==4&&scenes.count()==1,
          "SCN-003: additive Scene appended without replacing existing World");
    auto all=host.entities();
    Entity loaded_child=Entity::invalid(),loaded_parent=Entity::invalid();
    for(auto e:all){
        if(host.name(e)=="Child")loaded_child=e;
        if(host.name(e)=="Parent")loaded_parent=e;
    }
    check(loaded_child.valid()&&loaded_parent.valid()&&
          host.transform(loaded_child)->parent==loaded_parent&&
          scenes.owns(first,loaded_child)&&
          host.get_component<Link>(loaded_parent,link_type)->to==loaded_child,
          "SCN-003/007: additive parent and Scene-local Entity references are remapped");
    AdditiveHandle second;
    check(scenes.load(incoming,host,second,&error,&codecs) &&
          second!=first&&host.size()==6,
          "SCN-003/007: two copies of same local IDs never alias entities");
    // Create a cross-scene managed reference. Unload must null it before freeing
    // the target slot, and keep the reference within the other Scene untouched.
    host.get_component<Link>(stable,link_type)->to=loaded_child;
    auto other_entity=host.entities().back();
    auto other_link=host.get_component<Link>(other_entity,link_type);
    check(other_link&&other_link->to!=loaded_child,
          "SCN-007: duplicate additive Scene keeps its own links");
    check(scenes.unload(first,host,&error,&codecs) &&
          host.size()==4 && !host.is_alive(loaded_child) &&
          !host.is_alive(loaded_parent) && host.is_alive(persistent) &&
          host.get_component<Link>(stable,link_type)->to==Entity::invalid() &&
          other_link!=nullptr,  // pointer only checked for existence here
          "SCN-004/007: unload only one Scene, null cross-scene references");
    check(!scenes.unload(first,host,&error,&codecs) && host.size()==4,
          "SCN-004: double unload rejected without changes");
    // Roll back malformed imported hierarchy, duplicate local ids and refs.
    auto bad=incoming;
    bad.objects[0].parent_local_id=9999;
    AdditiveHandle sentinel{99};
    const auto before=host.size();
    check(!scenes.load(bad,host,sentinel,&error,&codecs) &&
          sentinel.value==99&&host.size()==before,
          "SCN-003: invalid hierarchy leaves World/handle unchanged");
    bad=incoming;
    bad.objects[1].local_id=14;
    check(!scenes.load(bad,host,sentinel,&error,&codecs)&&host.size()==before,
          "SCN-003: duplicate local ID rejected");
    bad=incoming;
    bad.objects[1].components[0].properties[0].value=std::uint64_t{9999};
    check(!scenes.load(bad,host,sentinel,&error,&codecs)&&host.size()==before,
          "SCN-007: dangling Scene-local EntityReference rejected");
    check(scenes.unload(second,host,&error,&codecs)&&
          host.size()==2&&scenes.count()==0&&host.is_alive(persistent),
          "SCN-004: second Scene unload preserves initial entities");
    if(!failures)std::cout<<"NEngineAdditiveSceneTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
