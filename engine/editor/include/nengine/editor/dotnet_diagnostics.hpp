#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace nengine::editor {
struct DotnetDiagnostic {
    bool is_error{};
    std::filesystem::path source{};
    std::uint32_t line{};
    std::uint32_t column{};
    std::string code{};
    std::string message{};
};
std::vector<DotnetDiagnostic> parse_dotnet_diagnostics(
    std::string_view output, const std::filesystem::path& project_root);
}
