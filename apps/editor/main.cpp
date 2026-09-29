#include <iostream>

#include "nengine/core/world.hpp"

int main() {
    nengine::core::World world;
    const auto camera = world.create("Main Camera");
    const auto cube = world.create("Cube");
    world.set_parent(cube, camera);

    std::cout << "NEngine Editor bootstrap 0.1.1\n";
    std::cout << "World online: " << world.size() << " objects\n";
    std::cout << "Next milestone: general component storage + prefab foundation\n";
    return 0;
}
