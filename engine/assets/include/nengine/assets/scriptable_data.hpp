#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <variant>

namespace nengine::assets {
// Simple, typed, versioned ScriptableData source (.nsdata). This format is
// engine-native; managed API bindings are tracked as a separate acceptance ID.
using ScriptableValue = std::variant<bool, std::int64_t, double, std::string>;
struct ScriptableData {
    std::unordered_map<std::string, ScriptableValue> values{};
    bool valid() const noexcept;
};
bool write_scriptable_data(std::ostream& output,const ScriptableData& source,
                           std::string* error=nullptr);
bool read_scriptable_data(std::istream& input,ScriptableData& target,
                          std::string* error=nullptr);
} // namespace nengine::assets
