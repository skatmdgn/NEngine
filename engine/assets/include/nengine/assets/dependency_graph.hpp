#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nengine/assets/asset_guid.hpp"

namespace nengine::assets {

class AssetDependencyGraph {
public:
    void set_dependencies(AssetGuid asset, std::vector<AssetGuid> dependencies);
    std::vector<AssetGuid> dependencies(AssetGuid asset) const;
    std::vector<AssetGuid> dependents(AssetGuid asset) const;
    void remove(AssetGuid asset);
    void clear();

private:
    using GuidSet = std::unordered_set<AssetGuid, AssetGuidHash>;

    std::unordered_map<AssetGuid, GuidSet, AssetGuidHash> forward_{};
    std::unordered_map<AssetGuid, GuidSet, AssetGuidHash> reverse_{};
};

} // namespace nengine::assets
