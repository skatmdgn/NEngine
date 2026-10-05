#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nengine::editor {

enum class LogSeverity : std::uint8_t {
    Trace,
    Info,
    Warning,
    Error,
};

struct ConsoleEntry {
    std::uint64_t sequence{0};
    LogSeverity severity{LogSeverity::Info};
    std::string source{};
    std::string message{};
    std::uint32_t repeat_count{1};
};

struct ConsoleFilter {
    bool trace{true};
    bool info{true};
    bool warning{true};
    bool error{true};
    std::string search{};
};

class ConsoleModel {
public:
    void push(
        LogSeverity severity,
        std::string source,
        std::string message,
        bool collapse_identical = true);

    void trace(std::string source, std::string message) {
        push(LogSeverity::Trace, std::move(source), std::move(message));
    }

    void info(std::string source, std::string message) {
        push(LogSeverity::Info, std::move(source), std::move(message));
    }

    void warning(std::string source, std::string message) {
        push(LogSeverity::Warning, std::move(source), std::move(message));
    }

    void error(std::string source, std::string message) {
        push(LogSeverity::Error, std::move(source), std::move(message));
    }

    const std::vector<ConsoleEntry>& entries() const noexcept {
        return entries_;
    }

    std::vector<ConsoleEntry> filtered(const ConsoleFilter& filter) const;

    std::size_t count(LogSeverity severity) const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }
    void clear() noexcept;

private:
    std::vector<ConsoleEntry> entries_{};
    std::uint64_t next_sequence_{1};
};

} // namespace nengine::editor
