// Acceptance regressions: CORE-004 parent topology, CORE-006 transactional
// malformed scene loads, CORE-008 generation-safe entity references.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "nengine/core/scene.hpp"
#include "nengine/core/world.hpp"

namespace {
int failures = 0;
void expect(bool value, const char* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct Link {
    nengine::core::Entity target{nengine::core::Entity::invalid()};
};

void check_parent_topology() {
    using namespace nengine::core;
    World world;
    const auto root = world.create("Root");
    const auto a = world.create("A");
    const auto b = world.create("B");
    const auto other = world.create("Other");
    expect(world.set_parent(a, root) && world.set_parent(b, a),
           "CORE-004: depth two parent chain succeeds");
    expect(!world.set_parent(root, b),
           "CORE-004: distant descendant cannot become ancestor's parent");
    expect(world.transform(root)->parent == Entity::invalid(),
           "CORE-004: rejected reparent leaves original parent untouched");
    expect(!world.set_parent(a, a),
           "CORE-004: self-parenting rejected");
    expect(world.transform(a)->parent == root,
           "CORE-004: rejected self cycle preserves original relationship");
    const auto dead = world.create("Dead");
    world.destroy(dead);
    expect(!world.set_parent(other, dead),
           "CORE-004: dead parent rejected");
    expect(world.transform(other)->parent == Entity::invalid(),
           "CORE-004: invalid parent does not change object");
    expect(world.set_parent(b, other),
           "CORE-004: reparent descendant to unrelated root");
    expect(world.transform(b)->parent == other && world.children(a).empty(),
           "CORE-004: hierarchy child enumeration follows new parent");
    expect(world.set_parent(b, Entity::invalid()),
           "CORE-004: explicit detach to root is supported");
    expect(world.transform(b)->parent == Entity::invalid(),
           "CORE-004: detach clears parent");
    World corrupt;
    auto c = corrupt.create("C");
    auto d = corrupt.create("D");
    auto e = corrupt.create("E");
    corrupt.transform(c)->parent = d;
    corrupt.transform(d)->parent = c;
    expect(!corrupt.set_parent(e, c),
           "CORE-004: already-corrupt cycle is bounded and rejected");
}

void check_transactional_scene_load() {
    using namespace nengine::core;
    World destination;
    const auto original = destination.create("DontDelete");
    destination.transform(original)->local_position = {3.f, 7.f, 11.f};
    const auto initial_count = destination.size();

    World source;
    source.create("NewScene");
    const SceneData valid = SceneSerializer::capture(source, "NewScene");
    SceneData parsed = valid;
    std::string error;

    const auto preserve = [&]() {
        return destination.size() == initial_count &&
               destination.is_alive(original) &&
               destination.name(original) == "DontDelete" &&
               destination.transform(original)->local_position ==
                   Vec3{3.f, 7.f, 11.f};
    };

    const std::string invalids[] = {
        "NENGINE_SCENE 3\nNAME \"Truncated\"\nOBJECTS 1\nOBJECT",
        "NENGINE_SCENE 3\nNAME \"OversizedCount\"\nOBJECTS 9999999999\n",
        "NENGINE_SCENE 3\nNAME \"Extra\"\nOBJECTS 0\nEND_SCENE\nJUNK\n",
        "NENGINE_SCENE 0\nNAME \"UnknownVersion\"\nOBJECTS 0\nEND_SCENE",
    };
    for (const auto& text : invalids) {
        std::stringstream stream(text);
        parsed = valid;
        expect(!SceneSerializer::read(stream, parsed, &error) &&
               parsed.name == valid.name && parsed.objects.size() == valid.objects.size(),
               "CORE-006: malformed stream leaves SceneData untouched");
        expect(preserve(), "CORE-006: malformed stream leaves live World intact");
    }

    const auto file = std::filesystem::temp_directory_path() /
                      "nengine_core_acceptance_corrupt.nscene";
    for (const auto& text : invalids) {
        {
            std::ofstream writer(file, std::ios::binary | std::ios::trunc);
            writer << text;
        }
        parsed = valid;
        expect(!SceneSerializer::load_file(file, parsed, &error) &&
               parsed.name == valid.name && parsed.objects.size() == valid.objects.size(),
               "CORE-006: corrupt disk scene is rejected without changing parsed data");
        expect(preserve(), "CORE-006: failed disk load preserves World");
    }
    {
        std::ofstream writer(file, std::ios::binary | std::ios::trunc);
        std::string block(1024 * 1024, 'x');
        for (int i = 0; i < 33; ++i) writer.write(block.data(), block.size());
    }
    parsed = valid;
    expect(!SceneSerializer::load_file(file, parsed, &error) &&
           parsed.name == valid.name,
           "CORE-006: 33 MiB on-disk scene is rejected transactionally");
    expect(preserve(), "CORE-006: oversized load preserves World");

    auto cyclic = valid;
    cyclic.objects.front().parent_local_id =
        static_cast<std::int64_t>(cyclic.objects.front().local_id);
    expect(!SceneSerializer::instantiate(cyclic, destination, &error),
           "CORE-006: malformed hierarchy does not instantiate");
    expect(preserve(), "CORE-006: instantiation failure preserves old World");
    std::error_code ignored;
    std::filesystem::remove(file, ignored);
}

void check_entity_reference_lifecycle() {
    using namespace nengine::core;
    World source;
    const auto target = source.create("Target");
    const auto owner = source.create("Owner");
    const auto type = ComponentRegistry::stable_id("Tests.AcceptanceLink");
    auto* link = source.add_component<Link>(owner, type);
    expect(link != nullptr, "CORE-008: link component attached");
    if (!link) return;
    link->target = target;
    World copy = source.clone();
    auto* copied = copy.get_component<Link>(owner, type);
    expect(copied && copied->target == target && copy.is_alive(copied->target),
           "CORE-008: cloned world preserves valid entity link");
    expect(source.destroy(target), "CORE-008: link target can be destroyed");
    expect(!source.is_alive(link->target),
           "CORE-008: destroyed link target is not resolved as alive");
    const auto reuse = source.create("SlotReused");
    expect(reuse.index() == target.index() && reuse != target &&
           !source.is_alive(link->target),
           "CORE-008: slot reuse cannot revalidate stale link");
    expect(copy.is_alive(target) && copied->target == target,
           "CORE-008: deleting source entity cannot invalidate clone entities");
    copied->target = Entity::invalid();
    expect(link->target == target,
           "CORE-008: cloned components do not alias original link data");

    World replacement;
    replacement.create("OtherScene");
    auto serialized = SceneSerializer::capture(replacement, "OtherScene");
    std::string error;
    expect(SceneSerializer::instantiate(serialized, source, &error),
           "CORE-008: scene reload replaces active world");
    expect(!source.is_alive(target) && !source.is_alive(owner) &&
           !source.is_alive(reuse),
           "CORE-008: all old handles are invalidated after scene reload");
    expect(source.size() == 1 && source.entities().size() == 1,
           "CORE-008: replacement world contains only reloaded entities");
}
} // namespace

int main() {
    check_parent_topology();
    check_transactional_scene_load();
    check_entity_reference_lifecycle();
    if (failures) std::cerr << failures << " core acceptance failures\n";
    else std::cout << "NEngineCoreAcceptanceTests: PASS\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
