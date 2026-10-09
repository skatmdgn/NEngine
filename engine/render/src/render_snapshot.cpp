#include "nengine/render/render_snapshot.hpp"

namespace nengine::render {

RenderSnapshot build_render_snapshot(
    const core::World& world) {

    RenderSnapshot result;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* transform =
            world.transform(entity);

        if (!transform) {
            continue;
        }

        if (const auto* camera =
                world.get_component<Camera>(
                    entity,
                    camera_type());
            camera && camera->enabled) {

            result.cameras.push_back({
                entity,
                *transform,
                world_matrix(
                    world,
                    entity),
                *camera
            });
        }

        if (const auto* light =
                world.get_component<Light>(
                    entity,
                    light_type());
            light && light->enabled) {

            result.lights.push_back({
                entity,
                *transform,
                world_matrix(
                    world,
                    entity),
                *light
            });
        }

        if (const auto* renderer =
                world.get_component<MeshRenderer>(
                    entity,
                    mesh_renderer_type());
            renderer && renderer->enabled) {

            result.meshes.push_back({
                entity,
                *transform,
                world_matrix(
                    world,
                    entity),
                *renderer
            });
        }

        if (const auto* renderer =
                world.get_component<SpriteRenderer>(
                    entity,
                    sprite_renderer_type());
            renderer && renderer->enabled) {

            result.sprites.push_back({
                entity,
                *transform,
                world_matrix(
                    world,
                    entity),
                *renderer
            });
        }
    }

    return result;
}

} // namespace nengine::render
