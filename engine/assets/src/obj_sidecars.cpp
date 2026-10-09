#include "nengine/assets/obj_sidecars.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nengine::assets {
namespace {

constexpr std::uint64_t kMaxObjTextBytes =
    64ull * 1024ull * 1024ull;

constexpr std::uint64_t kMaxMtlTextBytes =
    16ull * 1024ull * 1024ull;

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::string_view trim(
    std::string_view text) noexcept {

    const auto first =
        text.find_first_not_of(
            " \t\r\n");

    if (first ==
        std::string_view::npos) {
        return {};
    }

    const auto last =
        text.find_last_not_of(
            " \t\r\n");

    return text.substr(
        first,
        last - first + 1u);
}

bool parse_fields(
    std::string_view line,
    std::vector<std::string>& fields) {

    fields.clear();

    std::size_t i = 0u;

    while (i < line.size()) {
        while (i < line.size() &&
               std::isspace(
                   static_cast<unsigned char>(
                       line[i]))) {
            ++i;
        }

        if (i >= line.size() ||
            line[i] == '#') {
            break;
        }

        std::string field;

        if (line[i] == '"') {
            ++i;

            bool closed = false;

            while (i < line.size()) {
                const char ch =
                    line[i++];

                if (ch == '"') {
                    closed = true;
                    break;
                }

                if (ch == '\\' &&
                    i < line.size() &&
                    (line[i] == '"' ||
                     line[i] == '\\')) {

                    field.push_back(
                        line[i++]);
                    continue;
                }

                if (static_cast<unsigned char>(
                        ch) < 0x20u &&
                    ch != '\t') {
                    return false;
                }

                field.push_back(ch);
            }

            if (!closed) {
                return false;
            }
        } else {
            const auto begin = i;

            while (i < line.size() &&
                   !std::isspace(
                       static_cast<unsigned char>(
                           line[i])) &&
                   line[i] != '#') {
                ++i;
            }

            field.assign(
                line.substr(
                    begin,
                    i - begin));
        }

        if (!field.empty()) {
            fields.push_back(
                std::move(field));
        }

        if (i < line.size() &&
            line[i] == '#') {
            break;
        }
    }

    return true;
}

bool is_below_or_equal(
    const std::filesystem::path& child,
    const std::filesystem::path& root) {

    const auto relative =
        child.lexically_relative(root);

    if (relative.empty()) {
        return false;
    }

    if (relative == ".") {
        return true;
    }

    if (relative.has_root_path()) {
        return false;
    }

    for (const auto& part :
         relative) {
        if (part == "..") {
            return false;
        }
    }

    return true;
}

std::optional<ObjSidecar>
resolve_obj_sidecar_impl(
    const std::filesystem::path& root_directory,
    const std::filesystem::path& base_directory,
    std::string_view raw_path,
    ObjSidecarKind kind,
    std::string* error) {

    if (raw_path.empty() ||
        raw_path.size() > 4096u) {

        set_error(
            error,
            "OBJ sidecar path is empty or too long");
        return std::nullopt;
    }

    std::string normalized{
        raw_path};

    for (auto& ch :
         normalized) {

        const auto value =
            static_cast<unsigned char>(
                ch);

        if (value < 0x20u) {
            set_error(
                error,
                "OBJ sidecar path contains control characters");
            return std::nullopt;
        }

        if (ch == '\\') {
            ch = '/';
        }
    }

    if (normalized.find(':') !=
            std::string::npos) {

        set_error(
            error,
            "OBJ sidecar path must be local and relative");
        return std::nullopt;
    }

    const auto relative =
        std::filesystem::u8path(
            normalized.begin(),
            normalized.end());

    if (relative.empty() ||
        relative.is_absolute() ||
        relative.has_root_path()) {

        set_error(
            error,
            "OBJ sidecar path must be relative");
        return std::nullopt;
    }

    for (const auto& part :
         relative) {

        if (part.empty() ||
            part == "." ||
            part == "..") {

            set_error(
                error,
                "OBJ sidecar path traversal is not allowed");
            return std::nullopt;
        }
    }

    std::error_code ec;

    const auto root =
        std::filesystem::canonical(
            root_directory,
            ec);

    if (ec) {
        set_error(
            error,
            "OBJ source directory is unavailable");
        return std::nullopt;
    }

    const auto resolved =
        std::filesystem::canonical(
            base_directory /
                relative,
            ec);

    if (ec ||
        !std::filesystem::
            is_regular_file(
                resolved,
                ec) ||
        ec ||
        !is_below_or_equal(
            resolved,
            root)) {

        set_error(
            error,
            "OBJ sidecar is missing, non-file or escapes the model directory");
        return std::nullopt;
    }

    const auto staged_relative =
        resolved.lexically_relative(
            root);

    if (staged_relative.empty() ||
        staged_relative == "." ||
        staged_relative.has_root_path()) {

        set_error(
            error,
            "OBJ sidecar could not be mapped below the model directory");
        return std::nullopt;
    }

    return ObjSidecar{
        staged_relative,
        resolved,
        kind
    };
}

bool read_text_lines(
    const std::filesystem::path& path,
    std::uint64_t max_bytes,
    std::vector<std::string>& lines,
    std::string* error) {

    std::error_code ec;
    const auto size =
        std::filesystem::file_size(
            path,
            ec);

    if (ec ||
        size == 0u ||
        size > max_bytes) {

        set_error(
            error,
            "OBJ/MTL text source is unavailable, empty or too large");
        return false;
    }

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open OBJ/MTL sidecar source");
        return false;
    }

    lines.clear();

    std::string line;

    while (std::getline(
               input,
               line)) {

        lines.push_back(
            std::move(line));

        line.clear();
    }

    if (!input.eof() &&
        input.fail()) {

        set_error(
            error,
            "could not read OBJ/MTL sidecar source");
        return false;
    }

    return true;
}

bool append_unique(
    std::vector<ObjSidecar>& sidecars,
    std::unordered_set<std::string>& seen,
    ObjSidecar sidecar) {

    const auto key =
        sidecar.relative_path
            .generic_string();

    if (!seen.insert(key).second) {
        return false;
    }

    sidecars.push_back(
        std::move(sidecar));

    return true;
}

std::string content_fingerprint(
    const std::vector<ObjSidecar>& sidecars) {

    std::ostringstream fingerprint;

    for (const auto& sidecar :
         sidecars) {

        std::ifstream input(
            sidecar.source_path,
            std::ios::binary);

        if (!input) {
            return
                ":obj-sidecar-hash-open-error";
        }

        std::uint64_t hash =
            14695981039346656037ull;

        std::uint64_t size = 0u;

        std::array<char, 64u * 1024u>
            buffer{};

        while (input) {
            input.read(
                buffer.data(),
                static_cast<std::streamsize>(
                    buffer.size()));

            const auto count =
                input.gcount();

            if (count <= 0) {
                break;
            }

            size +=
                static_cast<std::uint64_t>(
                    count);

            for (std::streamsize i = 0;
                 i < count;
                 ++i) {

                hash ^=
                    static_cast<unsigned char>(
                        buffer[
                            static_cast<std::size_t>(
                                i)]);

                hash *=
                    1099511628211ull;
            }
        }

        if (!input.eof()) {
            return
                ":obj-sidecar-hash-read-error";
        }

        fingerprint
            << ":obj-sidecar:"
            << sidecar.relative_path
                .generic_string()
            << ':'
            << static_cast<int>(
                sidecar.kind)
            << ':'
            << size
            << ':'
            << std::hex
            << std::setw(16)
            << std::setfill('0')
            << hash
            << std::dec;
    }

    return fingerprint.str();
}

} // namespace

std::optional<ObjSidecar>
resolve_obj_sidecar(
    const std::filesystem::path& root_directory,
    const std::filesystem::path& base_directory,
    std::string_view raw_path,
    ObjSidecarKind kind,
    std::string* error) {

    return resolve_obj_sidecar_impl(
        root_directory,
        base_directory,
        raw_path,
        kind,
        error);
}

bool collect_obj_sidecars(
    const std::filesystem::path& source_obj,
    std::vector<ObjSidecar>& sidecars,
    std::string* error) {

    sidecars.clear();

    std::vector<std::string>
        obj_lines;

    if (!read_text_lines(
            source_obj,
            kMaxObjTextBytes,
            obj_lines,
            error)) {

        return false;
    }

    const auto root =
        source_obj.parent_path();

    std::unordered_set<std::string>
        seen;

    std::vector<ObjSidecar>
        material_libraries;

    std::vector<std::string>
        fields;

    for (const auto& line :
         obj_lines) {

        const auto view =
            trim(line);

        if (view.empty() ||
            view.front() == '#') {
            continue;
        }

        if (!parse_fields(
                view,
                fields) ||
            fields.empty()) {

            set_error(
                error,
                "malformed OBJ dependency directive");
            return false;
        }

        if (fields.front() !=
            "mtllib") {
            continue;
        }

        if (fields.size() < 2u) {
            set_error(
                error,
                "OBJ mtllib directive has no material library");
            return false;
        }

        for (std::size_t i = 1u;
             i < fields.size();
             ++i) {

            auto resolved =
                resolve_obj_sidecar_impl(
                    root,
                    root,
                    fields[i],
                    ObjSidecarKind::
                        MaterialLibrary,
                    error);

            if (!resolved) {
                return false;
            }

            if (append_unique(
                    sidecars,
                    seen,
                    *resolved)) {

                material_libraries.push_back(
                    std::move(
                        *resolved));
            }
        }
    }

    for (const auto& library :
         material_libraries) {

        std::vector<std::string>
            mtl_lines;

        if (!read_text_lines(
                library.source_path,
                kMaxMtlTextBytes,
                mtl_lines,
                error)) {

            return false;
        }

        for (const auto& line :
             mtl_lines) {

            const auto view =
                trim(line);

            if (view.empty() ||
                view.front() == '#') {
                continue;
            }

            if (!parse_fields(
                    view,
                    fields) ||
                fields.empty()) {

                set_error(
                    error,
                    "malformed MTL dependency directive");
                return false;
            }

            if (fields.front() !=
                "map_Kd") {
                continue;
            }

            if (fields.size() < 2u) {
                set_error(
                    error,
                    "MTL map_Kd directive has no image path");
                return false;
            }

            // Standard MTL map options precede the path; the final field is
            // the common portable representation. Quoted paths preserve
            // spaces through parse_fields().
            const auto& image_path =
                fields.back();

            auto resolved =
                resolve_obj_sidecar_impl(
                    root,
                    library.source_path
                        .parent_path(),
                    image_path,
                    ObjSidecarKind::
                        DiffuseTexture,
                    error);

            if (!resolved) {
                return false;
            }

            append_unique(
                sidecars,
                seen,
                std::move(
                    *resolved));
        }
    }

    std::sort(
        sidecars.begin(),
        sidecars.end(),
        [](const auto& left,
           const auto& right) {

            const auto left_path =
                left.relative_path
                    .generic_string();

            const auto right_path =
                right.relative_path
                    .generic_string();

            if (left_path !=
                right_path) {
                return
                    left_path <
                    right_path;
            }

            return
                static_cast<int>(
                    left.kind) <
                static_cast<int>(
                    right.kind);
        });

    return true;
}

std::string obj_sidecar_fingerprint(
    const std::filesystem::path& source_obj) {

    std::vector<ObjSidecar>
        sidecars;

    std::string error;

    if (!collect_obj_sidecars(
            source_obj,
            sidecars,
            &error)) {

        return
            ":obj-sidecar-error:" +
            error;
    }

    return
        content_fingerprint(
            sidecars);
}

} // namespace nengine::assets
