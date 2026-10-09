#include "nengine/render/obj_material.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nengine/assets/obj_sidecars.hpp"
#include "nengine/render/obj_mesh.hpp"

namespace nengine::render {
namespace {

constexpr std::uint64_t kMaxMtlBytes =
    32ull * 1024ull * 1024ull;

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
               (line[i] == ' ' ||
                line[i] == '\t' ||
                line[i] == '\r' ||
                line[i] == '\n')) {
            ++i;
        }

        if (i >= line.size() ||
            line[i] == '#') {
            break;
        }

        std::string field;

        if (line[i] == '"') {
            ++i;

            while (i < line.size() &&
                   line[i] != '"') {

                field.push_back(
                    line[i]);
                ++i;
            }

            if (i >= line.size()) {
                return false;
            }

            ++i;
        } else {
            const auto begin = i;

            while (i < line.size() &&
                   line[i] != ' ' &&
                   line[i] != '\t' &&
                   line[i] != '\r' &&
                   line[i] != '\n' &&
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

bool parse_float(
    std::string_view text,
    float& value) noexcept {

    if (text.empty()) {
        return false;
    }

    const auto* begin =
        text.data();

    const auto* end =
        begin + text.size();

    const auto result =
        std::from_chars(
            begin,
            end,
            value,
            std::chars_format::general);

    return result.ec ==
            std::errc{} &&
        result.ptr == end &&
        std::isfinite(value);
}

float saturate(
    float value) noexcept {

    return std::clamp(
        value,
        0.0f,
        1.0f);
}

struct Definition {
    std::array<float, 3> diffuse{
        1.0f, 1.0f, 1.0f};
    float alpha{1.0f};
    std::optional<std::filesystem::path>
        diffuse_texture{};
};

bool read_material_library(
    const ResolvedModelAsset& asset,
    const assets::ObjSidecar& library,
    std::unordered_map<
        std::string,
        Definition>& definitions,
    std::string* error) {

    std::error_code ec;

    const auto bytes =
        std::filesystem::file_size(
            library.source_path,
            ec);

    if (ec ||
        bytes == 0u ||
        bytes > kMaxMtlBytes) {

        set_error(
            error,
            "MTL source is unavailable, empty or too large");
        return false;
    }

    std::ifstream input(
        library.source_path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open MTL source");
        return false;
    }

    Definition* current = nullptr;
    std::string line;
    std::size_t line_number = 0u;
    std::vector<std::string> fields;

    while (std::getline(
               input,
               line)) {

        ++line_number;

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
                "malformed MTL line " +
                    std::to_string(
                        line_number));
            return false;
        }

        const auto& directive =
            fields.front();

        if (directive ==
            "newmtl") {

            if (fields.size() < 2u) {
                set_error(
                    error,
                    "MTL newmtl name is missing");
                return false;
            }

            current =
                &definitions[
                    fields[1]];

            *current =
                Definition{};

            continue;
        }

        if (!current) {
            continue;
        }

        if (directive == "Kd") {
            if (fields.size() < 4u) {
                set_error(
                    error,
                    "MTL Kd requires three components");
                return false;
            }

            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;

            if (!parse_float(
                    fields[1],
                    r) ||
                !parse_float(
                    fields[2],
                    g) ||
                !parse_float(
                    fields[3],
                    b)) {

                set_error(
                    error,
                    "MTL Kd components are invalid");
                return false;
            }

            current->diffuse = {
                saturate(r),
                saturate(g),
                saturate(b)
            };

            continue;
        }

        if (directive == "d" ||
            directive == "Tr") {

            if (fields.size() < 2u) {
                set_error(
                    error,
                    "MTL transparency directive is missing a value");
                return false;
            }

            float value = 0.0f;

            if (!parse_float(
                    fields[1],
                    value)) {

                set_error(
                    error,
                    "MTL transparency value is invalid");
                return false;
            }

            value =
                saturate(
                    value);

            current->alpha =
                directive == "Tr"
                    ? 1.0f - value
                    : value;

            continue;
        }

        if (directive == "map_Kd") {
            if (fields.size() < 2u) {
                set_error(
                    error,
                    "MTL map_Kd path is missing");
                return false;
            }

            const auto resolved =
                assets::resolve_obj_sidecar(
                    asset.source_path
                        .parent_path(),
                    library.source_path
                        .parent_path(),
                    fields.back(),
                    assets::ObjSidecarKind::
                        DiffuseTexture,
                    error);

            if (!resolved) {
                return false;
            }

            current->diffuse_texture =
                resolved->source_path;
        }
    }

    if (!input.eof() &&
        input.fail()) {

        set_error(
            error,
            "MTL source read failed");
        return false;
    }

    return true;
}

} // namespace

bool load_obj_material_sources(
    const ResolvedModelAsset& asset,
    std::vector<ObjCookedMaterialSource>& materials,
    std::string* error) {

    materials.clear();

    if (!asset.guid.valid()) {
        set_error(
            error,
            "OBJ material source model AssetGuid is invalid");
        return false;
    }

    std::vector<ObjMaterialSlot>
        slots;

    if (!discover_obj_material_slots(
            asset,
            slots,
            error)) {
        return false;
    }

    if (slots.empty()) {
        return true;
    }

    std::vector<assets::ObjSidecar>
        sidecars;

    if (!assets::collect_obj_sidecars(
            asset.source_path,
            sidecars,
            error)) {
        return false;
    }

    std::unordered_map<
        std::string,
        Definition>
        definitions;

    for (const auto& sidecar :
         sidecars) {

        if (sidecar.kind !=
            assets::ObjSidecarKind::
                MaterialLibrary) {
            continue;
        }

        if (!read_material_library(
                asset,
                sidecar,
                definitions,
                error)) {
            return false;
        }
    }

    materials.reserve(
        slots.size());

    for (const auto& slot :
         slots) {

        const auto found =
            definitions.find(
                slot.name);

        if (found ==
            definitions.end()) {
            continue;
        }

        ObjCookedMaterialSource
            material;

        material.slot =
            slot.slot;
        material.name =
            slot.name;
        material.diffuse =
            found->second.diffuse;
        material.alpha =
            found->second.alpha;
        material.diffuse_texture =
            found->second
                .diffuse_texture;

        materials.push_back(
            std::move(
                material));
    }

    return true;
}

} // namespace nengine::render
