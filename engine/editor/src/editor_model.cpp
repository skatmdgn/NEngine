#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/audio_integration.hpp"
#include "nengine/editor/render_integration.hpp"
#include "nengine/editor/physics_integration.hpp"
#include "nengine/editor/scripting_integration.hpp"
#include "nengine/physics/simulation.hpp"
#include "nengine/ui/canvas_serialization.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include "nengine/render/components.hpp"
#include <utility>

namespace nengine::editor {
namespace {

void bind_managed_property_access(
    scripting::ManagedRuntime& runtime,
    PropertyAccessRegistry& properties) {

    runtime.bind_property_access(
        &properties,
        [](
            void* context,
            const core::World& world,
            core::Entity entity,
            std::string_view component_name,
            std::string_view property_name,
            core::PropertyValue& output) {

            auto* registry =
                static_cast<
                    PropertyAccessRegistry*>(
                        context);

            if (!registry) {
                return false;
            }

            const auto value =
                registry->read(
                    world,
                    entity,
                    core::ComponentRegistry::stable_id(
                        component_name),
                    property_name);

            if (!value) {
                return false;
            }

            output = *value;
            return true;
        },
        [](
            void* context,
            core::World& world,
            core::Entity entity,
            std::string_view component_name,
            std::string_view property_name,
            const core::PropertyValue& value) {

            auto* registry =
                static_cast<
                    PropertyAccessRegistry*>(
                        context);

            return registry &&
                registry->write(
                    world,
                    entity,
                    core::ComponentRegistry::stable_id(
                        component_name),
                    property_name,
                    value);
        });
}

void bind_managed_physics_queries(
    scripting::ManagedRuntime& runtime) {

    runtime.bind_physics_queries(
        nullptr,
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 origin,
            core::Vec3 direction,
            float max_distance,
            bool include_triggers,
            std::uint32_t layer_mask,
            core::Entity& hit_entity,
            core::Vec3& point,
            core::Vec3& normal,
            float& distance,
            bool& is_trigger) {

            std::optional<
                physics::RaycastHit>
                hit;

            if (is_2d) {
                hit =
                    physics::raycast_2d(
                        world,
                        {origin.x, origin.y},
                        {direction.x, direction.y},
                        max_distance,
                        include_triggers,
                        layer_mask);
            } else {
                hit =
                    physics::raycast(
                        world,
                        origin,
                        direction,
                        max_distance,
                        include_triggers,
                        layer_mask);
            }

            if (!hit) {
                return false;
            }

            hit_entity = hit->entity;
            point = hit->point;
            normal = hit->normal;
            distance = hit->distance;
            is_trigger = hit->is_trigger;
            return true;
        },
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 center,
            core::Vec3 size,
            bool include_triggers,
            std::uint32_t layer_mask,
            std::uint64_t* output,
            std::size_t capacity) {

            std::vector<core::Entity>
                hits;

            if (is_2d) {
                hits =
                    physics::overlap_box_2d(
                        world,
                        {center.x, center.y},
                        {size.x, size.y},
                        include_triggers,
                        layer_mask);
            } else {
                hits =
                    physics::overlap_box(
                        world,
                        center,
                        size,
                        include_triggers,
                        layer_mask);
            }

            const auto count =
                std::min(
                    capacity,
                    hits.size());

            for (std::size_t index = 0;
                 index < count;
                 ++index) {
                output[index] =
                    hits[index].value;
            }

            return hits.size();
        },
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 origin,
            core::Vec3 size,
            core::Vec3 direction,
            float max_distance,
            bool include_triggers,
            std::uint32_t layer_mask,
            core::Entity& hit_entity,
            core::Vec3& point,
            core::Vec3& normal,
            float& distance,
            bool& is_trigger) {

            std::optional<
                physics::RaycastHit>
                hit;

            if (is_2d) {
                hit =
                    physics::box_cast_2d(
                        world,
                        {origin.x, origin.y},
                        {size.x, size.y},
                        {direction.x, direction.y},
                        max_distance,
                        include_triggers,
                        layer_mask);
            } else {
                hit =
                    physics::box_cast(
                        world,
                        origin,
                        size,
                        direction,
                        max_distance,
                        include_triggers,
                        layer_mask);
            }

            if (!hit) {
                return false;
            }

            hit_entity = hit->entity;
            point = hit->point;
            normal = hit->normal;
            distance = hit->distance;
            is_trigger = hit->is_trigger;
            return true;
        });
}

} // namespace

EditorModel::EditorModel() {
    component_registry_.register_type(
        "NEngine.Transform",
        "Core",
        true,
        false);

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Position",
            core::PropertyKind::Vec3,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Rotation",
            core::PropertyKind::Quaternion,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Scale",
            core::PropertyKind::Vec3,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Position",
        core::PropertyKind::Vec3,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_position};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Vec3>(&value);
            if (!transform || !typed) return false;
            transform->local_position = *typed;
            return true;
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Rotation",
        core::PropertyKind::Quaternion,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_rotation};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Quat>(&value);
            if (!transform || !typed) return false;
            transform->local_rotation = *typed;
            return true;
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Scale",
        core::PropertyKind::Vec3,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_scale};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Vec3>(&value);
            if (!transform || !typed) return false;
            transform->local_scale = *typed;
            return true;
        });

    if (!register_render_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Render component integration was only partially registered.");
    }

    if (!register_render_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Render component factories were only partially registered.");
    }

    if (!register_physics_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Physics component integration was only partially registered.");
    }

    if (!register_physics_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Physics component factories were only partially registered.");
    }

    if (!register_audio_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Audio component integration was only partially registered.");
    }

    if (!register_audio_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Audio component factories were only partially registered.");
    }

    if (!register_scripting_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Scripting component integration was only partially registered.");
    }

    if (!register_scripting_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Scripting component factories were only partially registered.");
    }

    managed_script_system_.bind(
        &managed_runtime_);

    console_.info(
        "Editor",
        "EditorModel initialized.");
}


bool EditorModel::load_navigation(assets::AssetGuid guid,
                                   std::string* error) {
    if (!can_edit()) {
        if (error) *error = "navigation cannot change during Play Mode";
        return false;
    }
    navigation::NavMesh loaded;
    if (!project_.load_nav_mesh(guid, loaded, error)) return false;
    navigation_ = std::move(loaded);
    return true;
}

bool EditorModel::begin_play_mode() {
    // Prefer an optional project Canvas convention without requiring
    // a new scene/component format for this diagnostic preview.
    if (!ui_canvas_asset_.valid()) {
        if (const auto* record =
                project_.assets().find_relative("UI/Main.ncanvas"))
            set_ui_canvas_asset(record->guid);
    }
    if (!play_session_.play(
            world_)) {
        return false;
    }

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
    sprite_controllers_.clear();
    bound_sprite_controllers_.clear();
    sprite_controller_asset_cache_.clear();
    if (auto* runtime = play_session_.runtime_world())
        sync_sprite_controllers(*runtime);
    audio_playback_system_.reset();
    audio_mix_snapshot_ = {};

    std::string audio_device_error;

    if (!audio_output_device_.open(
            &audio_device_error)) {

        console_.warning(
            "Audio",
            audio_device_error.empty()
                ? "Audio output device could not be opened."
                : std::move(audio_device_error));
    } else {
        const auto info =
            audio_output_device_.info();

        if (!info.diagnostic.empty()) {
            console_.warning(
                "Audio",
                info.diagnostic);
        } else {
            console_.info(
                "Audio",
                "Audio output: " +
                    info.backend +
                    " " +
                    std::to_string(
                        info.sample_rate) +
                    " Hz, " +
                    std::to_string(
                        info.channels) +
                    " channel(s).");
        }
    }

    if (managed_runtime_.valid() &&
        !managed_runtime_.reset_time()) {

        console_.warning(
            "Scripting",
            managed_runtime_.diagnostic());
    }

    return true;
}

bool EditorModel::stop_play_mode() {
    ui_clicked_widgets_.clear();
    ui_keyboard_router_.reset();
    ui_pointer_router_.reset();
    ui_events_.clear();
    sprite_controllers_.clear();
    bound_sprite_controllers_.clear();
    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    return play_session_.stop();
}

bool EditorModel::initialize_managed_runtime(
    const std::filesystem::path& hostfxr_path,
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    managed_runtime_.shutdown();

    if (!managed_runtime_.initialize(
            hostfxr_path,
            runtime_config_path,
            assembly_path,
            assembly_name)) {

        if (error) {
            *error =
                managed_runtime_
                    .diagnostic();
        }

        return false;
    }

    managed_runtime_.bind_input(
        &input_state_);
    bind_managed_ui();

    bind_managed_property_access(
        managed_runtime_,
        property_access_);

    bind_managed_physics_queries(
        managed_runtime_);

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

bool EditorModel::reload_managed_runtime(
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    if (!managed_runtime_.valid()) {
        if (error) {
            *error =
                "managed runtime is not initialized";
        }

        return false;
    }

    // Managed instances hold types from the collectible gameplay context.
    // Destroy them before requesting unload; Play Mode World state remains.
    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
    audio_playback_system_.reset();
    audio_mix_snapshot_ = {};

    if (!managed_runtime_.reload_gameplay(
            assembly_path,
            assembly_name)) {

        if (error) {
            *error =
                managed_runtime_
                    .diagnostic();
        }

        return false;
    }

    managed_runtime_.bind_input(
        &input_state_);
    bind_managed_ui();

    bind_managed_property_access(
        managed_runtime_,
        property_access_);

    bind_managed_physics_queries(
        managed_runtime_);

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

void EditorModel::shutdown_managed_runtime()
    noexcept {

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    managed_runtime_.shutdown();
}

void EditorModel::bind_managed_ui() {
    managed_runtime_.bind_ui(
        this,
        [](void* context, std::string_view label) -> std::uint64_t {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return ui::invalid_widget;
            const auto named = editor.ui_canvas_->find_by_name(label);
            if (named != ui::invalid_widget) return named;
            // Legacy .ncanvas v1 widgets remain discoverable by text.
            return editor.ui_canvas_->find_by_text(label);
        },
        [](void* context, std::uint64_t id, int operation) -> int {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return -1;
            const auto* widget = editor.ui_canvas_->find(id);
            if (!widget) return -1;
            switch (operation) {
                case 0: return widget->visible ? 1 : 0;
                case 1: return widget->interactable ? 1 : 0;
                case 2: return editor.ui_clicked_widgets_.contains(id) ? 1 : 0;
                default: return -1;
            }
        },
        [](void* context, std::uint64_t id, int operation, int value) -> int {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return -1;
            if (value != 0 && value != 1) return -1;
            if (operation == 0)
                return editor.ui_canvas_->set_visible(id, value != 0) ? 1 : 0;
            if (operation == 1)
                return editor.ui_canvas_->set_interactable(id, value != 0) ? 1 : 0;
            return -1;
        },
        [](void* context, std::uint64_t id) -> std::optional<std::string> {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return std::nullopt;
            const auto* widget = editor.ui_canvas_->find(id);
            if (!widget) return std::nullopt;
            return widget->text;
        },
        [](void* context, std::uint64_t id, std::string_view value) -> bool {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return false;
            return editor.ui_canvas_->set_text(id, std::string(value));
        },
        [](void* context, std::uint64_t id) -> std::optional<std::string> {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return std::nullopt;
            const auto* widget = editor.ui_canvas_->find(id);
            if (!widget || widget->kind != ui::WidgetKind::Image)
                return std::nullopt;
            return widget->image_texture.valid()
                ? widget->image_texture.to_string() : std::string{};
        },
        [](void* context, std::uint64_t id, std::string_view guid) -> bool {
            auto& editor = *static_cast<EditorModel*>(context);
            if (!editor.play_session_.is_playing() || !editor.ui_canvas_)
                return false;
            if (guid.empty())
                return editor.ui_canvas_->set_image_texture(id, {});
            if (guid.size() != 32u) return false;
            const auto parsed = assets::AssetGuid::parse(guid);
            return parsed && parsed->valid() &&
                   editor.ui_canvas_->set_image_texture(id, *parsed);
        });
}

bool EditorModel::begin_ui_canvas_edit(
    assets::AssetGuid asset, std::string* error) {
    if (!can_edit() || !asset.valid() || !project_.is_open()) {
        if (error) *error = "Canvas editing requires an open project in Edit Mode";
        return false;
    }
    if (ui_edit_open_ && ui_edit_history_.dirty()) {
        if (error) *error = "Canvas has unsaved authoring edits";
        return false;
    }
    const auto* record = project_.assets().find(asset);
    if (!record || record->importer_id != "NEngine.Canvas") {
        if (error) *error = "Canvas asset must be an imported .ncanvas file";
        return false;
    }
    const auto artifacts = project_.cached_artifacts(asset);
    if (!artifacts) {
        if (error) *error = "Canvas asset has no imported artifacts";
        return false;
    }
    std::string load_error;
    const auto* canvas = canvas_cache_.load(asset, *artifacts, &load_error);
    if (!canvas || !ui_edit_history_.reset(*canvas)) {
        if (error) *error = load_error.empty()
            ? "Canvas authoring draft could not be initialized" : load_error;
        return false;
    }
    ui_edit_asset_ = asset;
    ui_edit_open_ = true;
    ui_edit_selected_ = ui::invalid_widget;
    ui_edit_selection_.clear();
    ui_edit_drag_active_ = false;
    ui_edit_preview_.reset();
    if (error) error->clear();
    return true;
}

ui::UiLayout EditorModel::ui_edit_layout(float width, float height) const {
    if (!has_ui_canvas_draft()) return {};
    return ui_edit_preview_
        ? ui_edit_preview_->layout(width, height)
        : ui_edit_history_.canvas().layout(width, height);
}

bool EditorModel::select_ui_widget(ui::WidgetId id) noexcept {
    if (!has_ui_canvas_draft() ||
        (id != ui::invalid_widget && !ui_edit_history_.canvas().find(id)))
        return false;
    if (ui_edit_drag_active_ && id != ui_edit_selected_)
        ui_edit_end_drag(false);
    ui_edit_selected_ = id;
    ui_edit_selection_.clear();
    if (id != ui::invalid_widget) ui_edit_selection_.push_back(id);
    return true;
}

bool EditorModel::toggle_ui_widget(ui::WidgetId id) noexcept {
    if (!has_ui_canvas_draft() || id == ui::invalid_widget ||
        !ui_edit_history_.canvas().find(id)) return false;
    if (ui_edit_drag_active_) ui_edit_end_drag(false);
    const auto found = std::find(
        ui_edit_selection_.begin(), ui_edit_selection_.end(), id);
    if (found != ui_edit_selection_.end()) {
        ui_edit_selection_.erase(found);
        ui_edit_selected_ = ui_edit_selection_.empty()
            ? ui::invalid_widget : ui_edit_selection_.back();
    } else {
        ui_edit_selection_.push_back(id);
        ui_edit_selected_ = id;
    }
    return true;
}

ui::WidgetId EditorModel::select_ui_widget_at(
    float width, float height, core::Vec2 pointer) {
    if (!has_ui_canvas_draft()) return ui::invalid_widget;
    const auto id = ui::hit_test_ui(ui_edit_layout(width, height), pointer);
    select_ui_widget(id);
    return id;
}

bool EditorModel::ui_edit_apply_anchor_preset(
    ui::AnchorPreset preset, float viewport_width, float viewport_height) {
    if (!ui_edit_widget() || ui_edit_drag_active_) return false;
    const auto* widget = ui_edit_widget();
    const auto layout = ui_edit_layout(viewport_width, viewport_height);
    const auto own = std::find_if(layout.items.begin(), layout.items.end(),
        [this](const ui::UiLayoutItem& item) {
            return item.id == ui_edit_selected_;
        });
    if (own == layout.items.end()) return false;
    ui::UiRect parent{0, 0, viewport_width, viewport_height};
    if (widget->parent != ui::invalid_widget) {
        const auto found = std::find_if(layout.items.begin(), layout.items.end(),
            [widget](const ui::UiLayoutItem& item) {
                return item.id == widget->parent;
            });
        if (found == layout.items.end()) return false;
        parent = found->rect;
    }
    const auto anchors = ui::apply_anchor_preset(parent, own->rect, preset);
    return anchors && ui_edit_set_anchors(*anchors);
}

bool EditorModel::ui_edit_begin_drag() {
    const auto* widget = ui_edit_widget();
    if (!widget || ui_edit_drag_active_) return false;
    ui_edit_drag_anchors_ = widget->anchors;
    ui_edit_drag_widget_ = ui_edit_selected_;
    ui_edit_drag_targets_.clear();
    const auto& canvas = ui_edit_history_.canvas();
    for (auto id : ui_edit_selection_) {
        const auto* node = canvas.find(id);
        if (!node) continue;
        // Parent movement already carries descendants in resolved layout.
        bool ancestor_selected = false;
        auto parent = node->parent;
        while (parent != ui::invalid_widget) {
            if (is_ui_widget_selected(parent)) {
                ancestor_selected = true;
                break;
            }
            const auto* ancestor = canvas.find(parent);
            if (!ancestor) break;
            parent = ancestor->parent;
        }
        if (!ancestor_selected)
            ui_edit_drag_targets_.emplace_back(id, node->anchors);
    }
    if (ui_edit_drag_targets_.empty()) return false;
    ui_edit_drag_active_ = true;
    ui_edit_preview_.reset();
    return true;
}

bool EditorModel::ui_edit_update_drag(core::Vec2 delta, bool resize) {
    if (!ui_edit_drag_active_ || !has_ui_canvas_draft()) return false;
    auto candidate = ui_edit_history_.canvas();
    for (const auto& [id, anchors] : ui_edit_drag_targets_) {
        // Resizing is always a primary-only operation, whereas translation
        // acts on all selected roots in one transaction.
        if (resize && id != ui_edit_drag_widget_) continue;
        const auto next = ui::drag_canvas_anchors(anchors, delta, resize);
        if (!next || !candidate.set_anchors(id, *next)) return false;
    }
    ui_edit_preview_ = std::move(candidate);
    return true;
}

bool EditorModel::ui_edit_end_drag(bool commit) {
    if (!ui_edit_drag_active_) return false;
    ui_edit_drag_active_ = false;
    ui_edit_drag_widget_ = ui::invalid_widget;
    ui_edit_drag_targets_.clear();
    auto staged = std::move(ui_edit_preview_);
    ui_edit_preview_.reset();
    return commit && staged && ui_edit_history_.commit(std::move(*staged));
}

bool EditorModel::ui_edit_set_name(std::string name) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_name(ui_edit_selected_, std::move(name))) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_text(std::string text) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_text(ui_edit_selected_, std::move(text))) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_visible(bool visible) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_visible(ui_edit_selected_, visible)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_interactable(bool interactable) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_interactable(ui_edit_selected_, interactable))
        return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_anchors(ui::RectAnchors anchors) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_anchors(ui_edit_selected_, anchors)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_image_texture(
    assets::AssetGuid texture) {
    if (!ui_edit_widget() || ui_edit_drag_active_ ||
        ui_edit_widget()->kind != ui::WidgetKind::Image) return false;
    if (texture == ui_edit_image_texture()) return false;
    if (texture.valid()) {
        const auto* record = project_.assets().find(texture);
        if (!record || record->importer_id != "NEngine.Texture")
            return false;
    }
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_image_texture(ui_edit_selected_, texture)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_color(ui::UiColor color) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_color(ui_edit_selected_, color)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

ui::WidgetId EditorModel::ui_edit_add_widget(
    ui::WidgetKind kind, ui::WidgetId parent) {
    if (!has_ui_canvas_draft()) return ui::invalid_widget;
    auto candidate = ui_edit_history_.canvas();
    const auto id = candidate.add(kind, parent);
    if (id == ui::invalid_widget ||
        !ui_edit_history_.commit(std::move(candidate)))
        return ui::invalid_widget;
    ui_edit_selected_ = id;
    ui_edit_selection_ = {id};
    return id;
}

ui::WidgetId EditorModel::ui_edit_duplicate_selected() {
    if (!ui_edit_widget()) return ui::invalid_widget;
    auto candidate = ui_edit_history_.canvas();
    const auto duplicate = candidate.duplicate_subtree(ui_edit_selected_);
    if (duplicate == ui::invalid_widget ||
        !ui_edit_history_.commit(std::move(candidate)))
        return ui::invalid_widget;
    ui_edit_selected_ = duplicate;
    ui_edit_selection_ = {duplicate};
    return duplicate;
}

bool EditorModel::ui_edit_remove_selected() {
    if (!has_ui_canvas_draft() || ui_edit_drag_active_ ||
        ui_edit_selection_.empty()) return false;
    auto candidate = ui_edit_history_.canvas();
    bool changed = false;
    for (auto id : ui_edit_selection_) {
        // Removing a selected parent also removes its descendants.
        if (candidate.find(id)) changed = candidate.remove(id) || changed;
    }
    if (!changed || !ui_edit_history_.commit(std::move(candidate))) return false;
    ui_edit_selected_ = ui::invalid_widget;
    ui_edit_selection_.clear();
    return true;
}

bool EditorModel::ui_edit_reparent_selected(ui::WidgetId parent) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.reparent(ui_edit_selected_, parent)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_reparent_preserve_geometry(
    ui::WidgetId parent, float width, float height) {
    if (!ui_edit_widget() || ui_edit_drag_active_ ||
        !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0 || height <= 0) return false;
    const auto old_layout = ui_edit_layout(width, height);
    const auto own = std::find_if(old_layout.items.begin(),
        old_layout.items.end(), [this](const ui::UiLayoutItem& item) {
            return item.id == ui_edit_selected_;
        });
    if (own == old_layout.items.end()) return false;
    const auto geometry = own->rect;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.reparent(ui_edit_selected_, parent)) return false;
    ui::UiRect parent_rect{0, 0, width, height};
    if (parent != ui::invalid_widget) {
        const auto next_layout = candidate.layout(width, height);
        const auto found = std::find_if(next_layout.items.begin(),
            next_layout.items.end(), [parent](const ui::UiLayoutItem& item) {
                return item.id == parent;
            });
        if (found == next_layout.items.end()) return false;
        parent_rect = found->rect;
    }
    auto* widget = candidate.find(ui_edit_selected_);
    if (!widget) return false;
    // Keep the existing anchor configuration but recompute offsets against
    // the new parent rectangle so reparenting does not teleport the widget.
    auto anchors = widget->anchors;
    anchors.offset_min = {
        geometry.x - parent_rect.x -
            parent_rect.width * anchors.anchor_min.x,
        geometry.y - parent_rect.y -
            parent_rect.height * anchors.anchor_min.y};
    anchors.offset_max = {
        geometry.x + geometry.width - parent_rect.x -
            parent_rect.width * anchors.anchor_max.x,
        geometry.y + geometry.height - parent_rect.y -
            parent_rect.height * anchors.anchor_max.y};
    return candidate.set_anchors(ui_edit_selected_, anchors) &&
           ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::ui_edit_set_sibling_index(std::size_t index) {
    if (!ui_edit_widget()) return false;
    auto candidate = ui_edit_history_.canvas();
    if (!candidate.set_sibling_index(ui_edit_selected_, index)) return false;
    return ui_edit_history_.commit(std::move(candidate));
}

bool EditorModel::save_ui_canvas_edit(std::string* error) {
    const auto fail = [error](std::string message) {
        if (error) *error = std::move(message);
        return false;
    };
    if (!has_ui_canvas_draft() || ui_edit_drag_active_ ||
        !ui_edit_asset_.valid() || !project_.is_open())
        return fail("Canvas editing and saving require Edit Mode");

    const auto* record = project_.assets().find(ui_edit_asset_);
    if (!record || record->importer_id != "NEngine.Canvas")
        return fail("Selected Canvas asset is missing or not a Canvas");

    const auto& source = record->source_path;
    std::error_code ec;
    const auto root = std::filesystem::canonical(project_.assets_path(), ec);
    if (ec) return fail("Cannot resolve project Assets directory");
    const auto resolved = std::filesystem::canonical(source, ec);
    if (ec || std::filesystem::is_symlink(source) ||
        !std::filesystem::is_regular_file(source))
        return fail("Canvas source must be a regular project file");
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || relative == "." ||
        *relative.begin() == ".." || relative.is_absolute())
        return fail("Canvas source is outside project Assets");

    const auto stamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto temporary = source;
    temporary += ".nengine-tmp-" + stamp;
    auto backup = source;
    backup += ".nengine-backup-" + stamp;
    if (std::filesystem::exists(temporary) ||
        std::filesystem::exists(backup))
        return fail("Canvas save temporary path already exists");

    std::string write_error;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output ||
            !ui::write_canvas(output, ui_edit_history_.canvas(),
                              &write_error)) {
            output.close();
            std::filesystem::remove(temporary, ec);
            return fail(write_error.empty()
                ? "Canvas save could not serialize draft" : write_error);
        }
        output.flush();
        if (!output.good()) {
            output.close();
            std::filesystem::remove(temporary, ec);
            return fail("Canvas save could not flush temporary file");
        }
    }

    std::filesystem::rename(source, backup, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return fail("Canvas save could not back up original source");
    }
    std::filesystem::rename(temporary, source, ec);
    if (ec) {
        std::error_code restore_error;
        std::filesystem::rename(backup, source, restore_error);
        std::filesystem::remove(temporary, restore_error);
        return fail("Canvas save failed replacing original file; backup retained if restoration failed");
    }
    std::filesystem::remove(backup, ec);

    ui_edit_history_.mark_saved();
    project_.refresh_assets();
    const auto imported = project_.import_asset(ui_edit_asset_);
    canvas_cache_.clear();
    if (ui_canvas_asset_ == ui_edit_asset_)
        set_ui_canvas_asset(ui_edit_asset_);
    if (!imported.success)
        return fail("Canvas saved, but asset reimport failed: " + imported.message);
    if (error) error->clear();
    return true;
}

bool EditorModel::ui_edit_undo() {
    if (ui_edit_drag_active_) ui_edit_end_drag(false);
    if (!has_ui_canvas_draft() || !ui_edit_history_.undo()) return false;
    std::erase_if(ui_edit_selection_, [this](ui::WidgetId id) {
        return !ui_edit_history_.canvas().find(id);
    });
    ui_edit_selected_ = ui_edit_selection_.empty()
        ? ui::invalid_widget : ui_edit_selection_.back();
    return true;
}

bool EditorModel::ui_edit_redo() {
    if (ui_edit_drag_active_) ui_edit_end_drag(false);
    if (!has_ui_canvas_draft() || !ui_edit_history_.redo()) return false;
    std::erase_if(ui_edit_selection_, [this](ui::WidgetId id) {
        return !ui_edit_history_.canvas().find(id);
    });
    ui_edit_selected_ = ui_edit_selection_.empty()
        ? ui::invalid_widget : ui_edit_selection_.back();
    return true;
}

bool EditorModel::set_ui_canvas_asset(assets::AssetGuid asset) {
    ui_canvas_asset_ = asset;
    ui_canvas_reload_pending_ = false;
    ui_canvas_.reset();
    ui_pointer_router_.reset();
    ui_keyboard_router_.reset();
    ui_events_.clear();
    ui_clicked_widgets_.clear();
    if (!asset.valid()) return true;
    const auto artifacts = project_.cached_artifacts(asset);
    if (!artifacts) return false;
    std::string error;
    const auto* canvas = canvas_cache_.load(asset, *artifacts, &error);
    if (!canvas) {
        console_.warning("UI", error.empty()
            ? "Could not load Canvas asset." : error);
        return false;
    }
    ui_canvas_ = *canvas;
    return true;
}

std::vector<ui::UiPointerEvent> EditorModel::route_ui_keyboard(
    float width, float height,
    bool tab_held, bool enter_held, bool reverse_tab,
    bool space_held, bool escape_held) {

    if (!play_session_.is_playing() || !ui_canvas_) return {};
    auto events = ui_keyboard_router_.update(
        ui_canvas_->layout(width, height),
        tab_held, enter_held, reverse_tab, space_held,
        escape_held);
    for (const auto& event : events)
        if (event.type == ui::UiPointerEventType::Click)
            ui_clicked_widgets_.insert(event.target);
    ui_events_.insert(ui_events_.end(), events.begin(), events.end());
    if (ui_events_.size() > 128u)
        ui_events_.erase(ui_events_.begin(),
            ui_events_.begin() +
                static_cast<std::ptrdiff_t>(ui_events_.size() - 128u));
    return events;
}

ui::UiLayout EditorModel::ui_layout(float width, float height) const {
    return play_session_.is_playing() && ui_canvas_
        ? ui_canvas_->layout(width, height) : ui::UiLayout{};
}

std::vector<ui::UiPointerEvent> EditorModel::route_ui_pointer(
    float width, float height, core::Vec2 pointer, bool pressed) {

    if (!play_session_.is_playing() || !ui_canvas_) return {};
    const auto layout = ui_canvas_->layout(width, height);
    auto events = ui_pointer_router_.update(layout, pointer, pressed);
    for (const auto& event : events) {
        if (event.type == ui::UiPointerEventType::Down)
            ui_keyboard_router_.set_focus(layout, event.target);
        if (event.type == ui::UiPointerEventType::Click)
            ui_clicked_widgets_.insert(event.target);
    }
    ui_events_.insert(ui_events_.end(), events.begin(), events.end());
    // Bound event history to avoid retaining unbounded hover/click traffic.
    if (ui_events_.size() > 128u)
        ui_events_.erase(ui_events_.begin(),
            ui_events_.begin() +
                static_cast<std::ptrdiff_t>(ui_events_.size() - 128u));
    return events;
}

void EditorModel::sync_sprite_controllers(core::World& runtime) {
    for (const auto entity : runtime.entities()) {
        const auto* animator = runtime.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        if (!animator || !animator->controller.valid()) {
            if (bound_sprite_controllers_.erase(entity.value))
                sprite_controllers_.unbind(entity);
            continue;
        }
        const auto existing = bound_sprite_controllers_.find(entity.value);
        if (existing != bound_sprite_controllers_.end() &&
            existing->second == animator->controller) {
            for (const auto& [name, value] : animator->bool_parameters)
                sprite_controllers_.set_bool(entity, name, value);
            continue;
        }

        sprite_controllers_.unbind(entity);
        bound_sprite_controllers_.erase(entity.value);
        const auto artifacts = project_.cached_artifacts(animator->controller);
        if (!artifacts) continue;
        std::string error;
        const auto* controller = sprite_controller_asset_cache_.load(
            animator->controller, *artifacts, &error);
        if (!controller) continue;
        if (sprite_controllers_.bind(entity, *controller)) {
            bound_sprite_controllers_.emplace(
                entity.value, animator->controller);
            for (const auto& [name, value] : animator->bool_parameters)
                sprite_controllers_.set_bool(entity, name, value);
        }
    }
    // Destroyed/reused generational entity handles cannot inherit bindings.
    for (auto it = bound_sprite_controllers_.begin();
         it != bound_sprite_controllers_.end();) {
        if (!runtime.is_alive(core::Entity{it->first})) {
            sprite_controllers_.unbind(core::Entity{it->first});
            it = bound_sprite_controllers_.erase(it);
        } else {
            ++it;
        }
    }
}

bool EditorModel::set_sprite_controller_bool(
    core::Entity entity, std::string parameter, bool value) {

    auto* world = play_session_.runtime_world();
    if (!world || !world->is_alive(entity)) return false;
    auto* animator = world->get_component<render::SpriteAnimator>(
        entity, render::sprite_animator_type());
    if (!animator || parameter.empty() || parameter.size() > 128u)
        return false;
    if (!sprite_controllers_.set_bool(entity, parameter, value))
        return false;
    animator->bool_parameters[std::move(parameter)] = value;
    return true;
}

void EditorModel::tick_runtime(
    double elapsed_seconds) {

    auto* runtime =
        play_session_.runtime_world();

    // File-watcher invalidation should refresh the selected Canvas asset,
    // not silently remove the entire UI for the rest of Play Mode.
    // Only one reload is attempted per invalidation to avoid log spam
    // when the replacement file is malformed or not yet imported.
    if (runtime && ui_canvas_reload_pending_) {
        const auto requested = ui_canvas_asset_;
        ui_canvas_reload_pending_ = false;
        set_ui_canvas_asset(requested);
    }

    if (!runtime) {
        managed_script_system_.clear(
            play_session_.runtime_world());
        physics_contact_tracker_.clear();
    physics_impulse_cache_.clear();
        audio_playback_system_.reset();
        audio_output_device_.close();
        audio_mix_snapshot_ = {};
        return;
    }

    const auto steps =
        play_session_
            .consume_simulation_steps(
                elapsed_seconds);

    const auto play_state =
        play_session_.state();

    const bool run_playing_frame =
        play_state == PlayState::Playing &&
        elapsed_seconds > 0.0;

    if (steps == 0u &&
        !run_playing_frame) {
        return;
    }

    const auto fixed_delta =
        static_cast<float>(
            play_session_
                .fixed_delta_seconds());

    std::string animation_error;
    std::string script_error;
    std::string audio_error;
    std::string audio_device_error;

    const auto resolve_audio_duration =
        [this, &audio_error](
            assets::AssetGuid guid)
            -> std::optional<float> {

            const auto artifacts =
                project_.cached_artifacts(
                    guid);

            if (!artifacts) {
                return std::nullopt;
            }

            std::string load_error;

            const auto* clip =
                audio_clip_cache_.load(
                    guid,
                    *artifacts,
                    &load_error);

            if (!clip) {
                if (audio_error.empty()) {
                    audio_error =
                        std::move(load_error);
                }
                return std::nullopt;
            }

            return clip->duration_seconds();
        };

    if (managed_runtime_.valid()) {
        managed_runtime_.bind_input(
            &input_state_);
    }

    const auto run_fixed_step =
        [&]() {
            if (managed_runtime_.valid()) {
                managed_script_system_
                    .fixed_update(
                        *runtime,
                        fixed_delta,
                        &script_error);
            }
            // FixedUpdate may have written Animator.SetBool; evaluate it in
            // this same simulation frame, before sampling sprite textures.
            sync_sprite_controllers(*runtime);
            sprite_controllers_.evaluate(*runtime);
            render::update_sprite_animators(
                *runtime,
                fixed_delta,
                sprite_animation_cache_,
                [this](assets::AssetGuid guid) {
                    return project_.cached_artifacts(guid);
                },
                &animation_error);

            const auto physics_frame =
                physics::step_physics(
                    *runtime,
                    fixed_delta,
                    {0.0f, -9.81f, 0.0f},
                    &physics_impulse_cache_);

            const auto contact_events =
                physics_contact_tracker_.update(
                    physics_frame
                        .collisions
                        .overlaps);

            if (managed_runtime_.valid()) {
                for (const auto& event :
                     contact_events) {

                    const int phase =
                        static_cast<int>(
                            event.phase);

                    if (!managed_script_system_
                            .dispatch_physics_event(
                                *runtime,
                                event.first,
                                event.second,
                                phase,
                                event.is_trigger,
                                event.is_2d,
                                event.normal,
                                event.penetration,
                                &script_error)) {
                        continue;
                    }

                    managed_script_system_
                        .dispatch_physics_event(
                            *runtime,
                            event.second,
                            event.first,
                            phase,
                            event.is_trigger,
                            event.is_2d,
                            {
                                -event.normal.x,
                                -event.normal.y,
                                -event.normal.z
                            },
                            event.penetration,
                            &script_error);
                }
            }
        };

    if (play_state ==
        PlayState::Paused) {

        // Each requested editor Step is one complete simulation frame.
        for (std::uint32_t step = 0u;
             step < steps;
             ++step) {

            run_fixed_step();

            if (managed_runtime_.valid()) {
                managed_script_system_.update(
                    *runtime,
                    fixed_delta,
                    &script_error);
            }

            audio_playback_system_.update(
                *runtime,
                fixed_delta,
                resolve_audio_duration);

            audio_mix_snapshot_ =
                audio::build_mix_snapshot(
                    *runtime);

            if (audio_output_device_.is_open()) {
                audio_output_device_.pump(
                    audio_mix_snapshot_,
                    [this](assets::AssetGuid guid) {
                        return audio_clip_cache_.find(
                            guid);
                    },
                    &audio_device_error);
            }
        }
    } else {
        // A host frame may contain zero or several fixed simulation steps.
        for (std::uint32_t step = 0u;
             step < steps;
             ++step) {

            run_fixed_step();
        }

        if (run_playing_frame) {

            const auto clamped_elapsed =
                elapsed_seconds > 0.25
                    ? 0.25
                    : elapsed_seconds;

            const auto frame_delta =
                static_cast<float>(
                    clamped_elapsed);

            if (managed_runtime_.valid()) {
                managed_script_system_.update(
                    *runtime,
                    frame_delta,
                    &script_error);
            }

            audio_playback_system_.update(
                *runtime,
                frame_delta,
                resolve_audio_duration);

            audio_mix_snapshot_ =
                audio::build_mix_snapshot(
                    *runtime);

            if (audio_output_device_.is_open()) {
                audio_output_device_.pump(
                    audio_mix_snapshot_,
                    [this](assets::AssetGuid guid) {
                        return audio_clip_cache_.find(
                            guid);
                    },
                    &audio_device_error);
            }
        }
    }

    // UI activation is a one-frame signal; do not replay stale clicks in
    // later managed Update calls (including after a paused Step).
    ui_clicked_widgets_.clear();

    if (!script_error.empty()) {
        console_.warning(
            "Scripting",
            std::move(
                script_error));
    }

    if (!animation_error.empty()) {
        console_.warning(
            "Animation",
            std::move(
                animation_error));
    }

    if (!audio_error.empty()) {
        console_.warning(
            "Audio",
            std::move(
                audio_error));
    }

    if (!audio_device_error.empty()) {
        console_.warning(
            "Audio",
            std::move(
                audio_device_error));
    }
}

} // namespace nengine::editor
