#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "nengine/core/component_registry.hpp"

namespace nengine::core {

class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void erase(std::uint32_t entity_index) = 0;
    virtual void clear() = 0;
    virtual bool contains(std::uint32_t entity_index) const noexcept = 0;
    virtual std::type_index value_type() const noexcept = 0;
    virtual std::unique_ptr<ComponentPoolBase> clone() const = 0;
};

template <typename T>
class ComponentPool final : public ComponentPoolBase {
public:
    T* get(std::uint32_t entity_index) noexcept {
        if (entity_index >= values_.size() || !values_[entity_index]) {
            return nullptr;
        }
        return &*values_[entity_index];
    }

    const T* get(std::uint32_t entity_index) const noexcept {
        if (entity_index >= values_.size() || !values_[entity_index]) {
            return nullptr;
        }
        return &*values_[entity_index];
    }

    template <typename... Args>
    T* emplace(std::uint32_t entity_index, Args&&... args) {
        if (entity_index >= values_.size()) {
            values_.resize(static_cast<std::size_t>(entity_index) + 1u);
        }
        if (values_[entity_index]) {
            return nullptr;
        }
        values_[entity_index].emplace(std::forward<Args>(args)...);
        return &*values_[entity_index];
    }

    void erase(std::uint32_t entity_index) override {
        if (entity_index < values_.size()) {
            values_[entity_index].reset();
        }
    }

    void clear() override { values_.clear(); }

    bool contains(std::uint32_t entity_index) const noexcept override {
        return entity_index < values_.size() && values_[entity_index].has_value();
    }

    std::type_index value_type() const noexcept override { return typeid(T); }

    std::unique_ptr<ComponentPoolBase> clone() const override {
        return std::make_unique<ComponentPool<T>>(*this);
    }

private:
    std::vector<std::optional<T>> values_{};
};

} // namespace nengine::core
