#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual bool execute(core::World& world) = 0;
    virtual void undo(core::World& world) = 0;
    virtual std::string name() const = 0;
};

class CommandStack {
public:
    bool execute(core::World& world, std::unique_ptr<EditorCommand> command);
    bool undo(core::World& world);
    bool redo(core::World& world);
    void clear() noexcept;

    bool can_undo() const noexcept { return cursor_ > 0; }
    bool can_redo() const noexcept { return cursor_ < history_.size(); }
    std::size_t size() const noexcept { return history_.size(); }
    std::size_t cursor() const noexcept { return cursor_; }
    std::string_view undo_name() const noexcept;
    std::string_view redo_name() const noexcept;

private:
    std::vector<std::unique_ptr<EditorCommand>> history_{};
    std::size_t cursor_{0};
};

class RenameEntityCommand final : public EditorCommand {
public:
    RenameEntityCommand(core::Entity entity, std::string new_name)
        : entity_(entity), new_name_(std::move(new_name)) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string name() const override { return "Rename Entity"; }

private:
    core::Entity entity_{};
    std::string old_name_{};
    std::string new_name_{};
    bool captured_{false};
};

class SetActiveCommand final : public EditorCommand {
public:
    SetActiveCommand(core::Entity entity, bool active)
        : entity_(entity), new_value_(active) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string name() const override { return "Set Active"; }

private:
    core::Entity entity_{};
    bool old_value_{true};
    bool new_value_{true};
    bool captured_{false};
};

class SetTransformCommand final : public EditorCommand {
public:
    SetTransformCommand(core::Entity entity, core::Transform value)
        : entity_(entity), new_value_(value) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string name() const override { return "Set Transform"; }

private:
    core::Entity entity_{};
    core::Transform old_value_{};
    core::Transform new_value_{};
    bool captured_{false};
};

} // namespace nengine::editor
