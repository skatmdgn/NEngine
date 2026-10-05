#include "nengine/assets/dependency_graph.hpp"

#include <algorithm>

namespace nengine::assets {

void AssetDependencyGraph::set_dependencies(
    AssetGuid asset,
    std::vector<AssetGuid> dependencies_value) {

    const auto existing = forward_.find(asset);
    if (existing != forward_.end()) {
        for (const auto dependency : existing->second) {
            auto reverse = reverse_.find(dependency);
            if (reverse != reverse_.end()) {
                reverse->second.erase(asset);
                if (reverse->second.empty()) reverse_.erase(reverse);
            }
        }
    }

    auto& forward = forward_[asset];
    forward.clear();

    for (const auto dependency : dependencies_value) {
        if (!dependency.valid() || dependency == asset) continue;
        forward.insert(dependency);
        reverse_[dependency].insert(asset);
    }

    if (forward.empty()) forward_.erase(asset);
}

std::vector<AssetGuid> AssetDependencyGraph::dependencies(AssetGuid asset) const {
    std::vector<AssetGuid> result;
    const auto it = forward_.find(asset);
    if (it == forward_.end()) return result;

    result.assign(it->second.begin(), it->second.end());
    std::sort(result.begin(), result.end(), [](AssetGuid a, AssetGuid b) {
        return a.to_string() < b.to_string();
    });
    return result;
}

std::vector<AssetGuid> AssetDependencyGraph::dependents(AssetGuid asset) const {
    std::vector<AssetGuid> result;
    const auto it = reverse_.find(asset);
    if (it == reverse_.end()) return result;

    result.assign(it->second.begin(), it->second.end());
    std::sort(result.begin(), result.end(), [](AssetGuid a, AssetGuid b) {
        return a.to_string() < b.to_string();
    });
    return result;
}

void AssetDependencyGraph::remove(AssetGuid asset) {
    set_dependencies(asset, {});

    const auto reverse = reverse_.find(asset);
    if (reverse != reverse_.end()) {
        const auto users = reverse->second;
        for (const auto user : users) {
            auto forward = forward_.find(user);
            if (forward != forward_.end()) {
                forward->second.erase(asset);
                if (forward->second.empty()) forward_.erase(forward);
            }
        }
        reverse_.erase(asset);
    }
}

void AssetDependencyGraph::clear() {
    forward_.clear();
    reverse_.clear();
}

} // namespace nengine::assets
