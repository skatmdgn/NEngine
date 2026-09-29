#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/core/world.hpp"

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace

int main() {
    using namespace nengine::core;

    World world;
    const auto parent = world.create("Parent");
    const auto child = world.create("Child");

    check(world.size() == 2, "create increments size");
    check(world.is_alive(parent), "new entity is alive");
    check(world.name(child) == "Child", "name roundtrip");

    check(world.set_parent(child, parent), "valid parenting succeeds");
    check(world.transform(child)->parent == parent, "parent stored");
    check(!world.set_parent(parent, child), "cycle is rejected");

    world.transform(parent)->local_position = {10.0f, 20.0f, 30.0f};
    world.transform(child)->local_position = {1.0f, 2.0f, 3.0f};
    check(world.transform(child)->local_position == Vec3{1.0f, 2.0f, 3.0f}, "transform data roundtrip");

    ComponentRegistry registry;
    check(registry.register_type("NEngine.Transform", "Core", true), "component registration succeeds");
    check(!registry.register_type("NEngine.Transform", "Core", true), "duplicate component name rejected");
    const auto transform_type = registry.find("NEngine.Transform");
    check(transform_type != nullptr, "component lookup by name succeeds");
    check(transform_type && registry.find(transform_type->id) == transform_type, "component lookup by id succeeds");

    const auto captured = SceneSerializer::capture(world, "CoreTest");
    check(captured.objects.size() == 2, "scene captures all objects");

    std::stringstream stream;
    std::string error;
    check(SceneSerializer::write(captured, stream, &error), "scene serializes to stream");

    SceneData loaded;
    check(SceneSerializer::read(stream, loaded, &error), "scene deserializes from stream");
    check(loaded.name == "CoreTest", "scene name roundtrip");
    check(loaded.objects.size() == 2, "scene object count roundtrip");

    World restored;
    check(SceneSerializer::instantiate(loaded, restored, &error), "scene instantiates into world");
    check(restored.size() == 2, "restored world has expected object count");

    Entity restored_parent = Entity::invalid();
    Entity restored_child = Entity::invalid();
    for (const auto entity : restored.entities()) {
        if (restored.name(entity) == "Parent") restored_parent = entity;
        if (restored.name(entity) == "Child") restored_child = entity;
    }
    check(restored_parent.valid() && restored_child.valid(), "restored entities located by name");
    check(restored.transform(restored_child)->parent == restored_parent, "scene hierarchy roundtrip");
    check(restored.transform(restored_parent)->local_position == Vec3{10.0f, 20.0f, 30.0f}, "parent transform roundtrip");
    check(restored.transform(restored_child)->local_position == Vec3{1.0f, 2.0f, 3.0f}, "child transform roundtrip");

    SceneData invalid_scene = loaded;
    invalid_scene.objects[0].parent_local_id = static_cast<std::int64_t>(invalid_scene.objects[1].local_id);
    invalid_scene.objects[1].parent_local_id = static_cast<std::int64_t>(invalid_scene.objects[0].local_id);
    World untouched;
    const auto sentinel = untouched.create("Sentinel");
    check(!SceneSerializer::instantiate(invalid_scene, untouched, &error), "cyclic serialized hierarchy rejected transactionally");
    check(untouched.size() == 1 && untouched.is_alive(sentinel), "failed scene load does not mutate destination world");

    const auto stale = child;
    check(world.destroy(child), "destroy succeeds");
    check(!world.is_alive(stale), "destroy invalidates old handle");

    const auto replacement = world.create("Replacement");
    check(replacement.index() == stale.index(), "slot reuse is deterministic");
    check(replacement.generation() != stale.generation(), "generation prevents stale handle reuse");

    check(world.destroy(parent), "parent destroy succeeds");
    check(world.transform(replacement)->parent == Entity::invalid(), "children are detached when parent dies");

    if (failures == 0) {
        std::cout << "NEngineCoreTests: PASS\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "NEngineCoreTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
