#include "nengine/editor/console_model.hpp"

#include <algorithm>
#include <cctype>

namespace nengine::editor {
namespace {

bool enabled(LogSeverity severity, const ConsoleFilter& filter) {
    switch (severity) {
    case LogSeverity::Trace: return filter.trace;
    case LogSeverity::Info: return filter.info;
    case LogSeverity::Warning: return filter.warning;
    case LogSeverity::Error: return filter.error;
    }
    return true;
}

std::string lower_copy(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

} // namespace

void ConsoleModel::push(
    LogSeverity severity,
    std::string source,
    std::string message,
    bool collapse_identical) {

    if (collapse_identical && !entries_.empty()) {
        auto& last = entries_.back();
        if (last.severity == severity &&
            last.source == source &&
            last.message == message) {
            ++last.repeat_count;
            return;
        }
    }

    entries_.push_back({
        next_sequence_++,
        severity,
        std::move(source),
        std::move(message),
        1
    });
}

std::vector<ConsoleEntry> ConsoleModel::filtered(
    const ConsoleFilter& filter) const {

    std::vector<ConsoleEntry> result;
    const auto search = lower_copy(filter.search);

    for (const auto& entry : entries_) {
        if (!enabled(entry.severity, filter)) continue;

        if (!search.empty()) {
            const auto haystack =
                lower_copy(entry.source + " " + entry.message);

            if (haystack.find(search) == std::string::npos) {
                continue;
            }
        }

        result.push_back(entry);
    }

    return result;
}

std::size_t ConsoleModel::count(LogSeverity severity) const noexcept {
    std::size_t result = 0;
    for (const auto& entry : entries_) {
        if (entry.severity == severity) {
            result += entry.repeat_count;
        }
    }
    return result;
}

void ConsoleModel::clear() noexcept {
    entries_.clear();
}

} // namespace nengine::editor
