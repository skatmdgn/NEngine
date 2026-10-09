#include "nengine/scripting/managed_build.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace nengine::scripting {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::filesystem::path dotnet_name() {
#if defined(_WIN32)
    return "dotnet.exe";
#else
    return "dotnet";
#endif
}

bool executable_candidate(
    const std::filesystem::path& path) {

    std::error_code error;

    return
        !path.empty() &&
        std::filesystem::is_regular_file(
            path,
            error) &&
        !error;
}

std::optional<std::filesystem::path>
from_dotnet_root() {

    const char* root =
        std::getenv(
            "DOTNET_ROOT");

#if defined(_WIN32)
    if (!root || !*root) {
        root =
            std::getenv(
                "DOTNET_ROOT(x86)");
    }
#endif

    if (!root || !*root) {
        return std::nullopt;
    }

    const auto candidate =
        std::filesystem::path{
            root} /
        dotnet_name();

    return executable_candidate(
               candidate)
        ? std::optional<
            std::filesystem::path>{
                std::filesystem::absolute(
                    candidate)}
        : std::nullopt;
}

std::optional<std::filesystem::path>
from_path_environment() {

    const char* raw =
        std::getenv(
            "PATH");

    if (!raw || !*raw) {
        return std::nullopt;
    }

#if defined(_WIN32)
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    std::string_view path{
        raw};

    std::size_t begin = 0u;

    while (begin <= path.size()) {
        const auto end =
            path.find(
                separator,
                begin);

        const auto part =
            end ==
                std::string_view::npos
                ? path.substr(
                    begin)
                : path.substr(
                    begin,
                    end - begin);

        if (!part.empty()) {
            const auto candidate =
                std::filesystem::path{
                    std::string{part}} /
                dotnet_name();

            if (executable_candidate(
                    candidate)) {
                return
                    std::filesystem::absolute(
                        candidate);
            }
        }

        if (end ==
            std::string_view::npos) {
            break;
        }

        begin =
            end + 1u;
    }

    return std::nullopt;
}

#if defined(_WIN32)
std::string shell_quote(
    std::string_view argument) {

    if (argument.empty()) {
        return "\"\"";
    }

    if (argument.find_first_of(
            " \t\"") ==
        std::string_view::npos) {
        return std::string{
            argument};
    }

    std::string quoted;
    quoted.push_back('"');

    std::size_t backslashes = 0u;

    for (const char ch :
         argument) {

        if (ch == '\\') {
            ++backslashes;
            continue;
        }

        if (ch == '"') {
            quoted.append(
                backslashes * 2u +
                    1u,
                '\\');

            quoted.push_back('"');
            backslashes = 0u;
            continue;
        }

        quoted.append(
            backslashes,
            '\\');

        backslashes = 0u;
        quoted.push_back(ch);
    }

    quoted.append(
        backslashes * 2u,
        '\\');

    quoted.push_back('"');
    return quoted;
}
#else
std::string shell_quote(
    std::string_view argument) {

    std::string quoted;
    quoted.push_back('\'');

    for (const char ch :
         argument) {

        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }

    quoted.push_back('\'');
    return quoted;
}
#endif

int normalize_exit_code(
    int raw) noexcept {

    if (raw < 0) {
        return raw;
    }

#if defined(_WIN32)
    return raw;
#else
    if (WIFEXITED(raw)) {
        return WEXITSTATUS(raw);
    }

    if (WIFSIGNALED(raw)) {
        return 128 +
            WTERMSIG(raw);
    }

    return raw;
#endif
}

} // namespace

std::optional<std::filesystem::path>
discover_dotnet_executable(
    const std::filesystem::path&
        explicit_path) {

    if (!explicit_path.empty()) {
        if (executable_candidate(
                explicit_path)) {
            return
                std::filesystem::absolute(
                    explicit_path);
        }

        if (std::filesystem::
                is_directory(
                    explicit_path)) {

            const auto candidate =
                explicit_path /
                dotnet_name();

            if (executable_candidate(
                    candidate)) {
                return
                    std::filesystem::absolute(
                        candidate);
            }
        }

        return std::nullopt;
    }

    if (auto root =
            from_dotnet_root()) {
        return root;
    }

    return
        from_path_environment();
}

std::optional<ManagedBuildPlan>
make_managed_build_plan(
    const ManagedBuildConfig& config,
    std::string* error) {

    if (config.project_path.empty() ||
        config.output_directory.empty() ||
        config.configuration.empty()) {

        set_error(
            error,
            "managed build requires project path, output directory and configuration");

        return std::nullopt;
    }

    std::error_code filesystem_error;

    if (!std::filesystem::
            is_regular_file(
                config.project_path,
                filesystem_error) ||
        filesystem_error) {

        set_error(
            error,
            "managed project file does not exist: " +
                config.project_path
                    .generic_string());

        return std::nullopt;
    }

    const auto executable =
        discover_dotnet_executable(
            config.dotnet_executable);

    if (!executable) {
        set_error(
            error,
            "dotnet executable was not found; install a .NET SDK or configure DOTNET_ROOT");

        return std::nullopt;
    }

    ManagedBuildPlan plan;
    plan.executable =
        *executable;

    plan.arguments = {
        "build",
        config.project_path
            .string(),
        "--configuration",
        config.configuration,
        "--output",
        config.output_directory
            .string(),
        "--nologo"
    };

    if (!config.restore) {
        plan.arguments.push_back(
            "--no-restore");
    }

    const auto assembly_name =
        config.project_path
            .stem()
            .string();

    plan.assembly_path =
        config.output_directory /
        (assembly_name + ".dll");

    plan.pdb_path =
        config.output_directory /
        (assembly_name + ".pdb");

    return plan;
}

ManagedBuildResult run_managed_build(
    const ManagedBuildConfig& config) {

    ManagedBuildResult result;
    std::string error;

    const auto plan =
        make_managed_build_plan(
            config,
            &error);

    if (!plan) {
        result.message =
            std::move(error);
        return result;
    }

    result.plan =
        *plan;

    std::error_code filesystem_error;

    std::filesystem::create_directories(
        config.output_directory,
        filesystem_error);

    if (filesystem_error) {
        result.message =
            "could not create managed build output directory: " +
            config.output_directory
                .generic_string();

        return result;
    }

    std::ostringstream command;
    command
        << shell_quote(
            result.plan.executable
                .string());

    for (const auto& argument :
         result.plan.arguments) {

        command
            << ' '
            << shell_quote(
                argument);
    }

    const auto raw_exit =
        std::system(
            command.str().c_str());

    result.exit_code =
        normalize_exit_code(
            raw_exit);

    if (result.exit_code != 0) {
        result.message =
            "dotnet build failed with exit code " +
            std::to_string(
                result.exit_code);

        return result;
    }

    if (!std::filesystem::
            is_regular_file(
                result.plan.assembly_path,
                filesystem_error) ||
        filesystem_error) {

        result.message =
            "dotnet build succeeded but managed assembly was not produced: " +
            result.plan.assembly_path
                .generic_string();

        return result;
    }

    result.success = true;
    result.message =
        "managed scripts built: " +
        result.plan.assembly_path
            .generic_string();

    return result;
}

} // namespace nengine::scripting
