#include "nengine/render/obj_mesh.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

constexpr std::uint64_t kMaxObjBytes =
    256ull * 1024ull * 1024ull;

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

std::vector<std::string_view>
tokens(
    std::string_view text) {

    std::vector<std::string_view> result;

    while (!text.empty()) {
        text = trim(text);

        if (text.empty() ||
            text.front() == '#') {
            break;
        }

        const auto separator =
            text.find_first_of(
                " \t\r\n");

        if (separator ==
            std::string_view::npos) {

            result.push_back(text);
            break;
        }

        result.push_back(
            text.substr(
                0,
                separator));

        text.remove_prefix(
            separator + 1u);
    }

    return result;
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

    auto result =
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

bool parse_index(
    std::string_view text,
    std::int64_t& value) noexcept {

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
            value);

    return result.ec ==
            std::errc{} &&
        result.ptr == end &&
        value != 0;
}

std::optional<std::size_t>
resolve_index(
    std::int64_t obj_index,
    std::size_t count) noexcept {

    if (obj_index > 0) {
        const auto zero_based =
            static_cast<std::uint64_t>(
                obj_index - 1);

        if (zero_based >= count) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(
            zero_based);
    }

    if (obj_index ==
        std::numeric_limits<
            std::int64_t>::min()) {
        return std::nullopt;
    }

    const auto from_end =
        static_cast<std::uint64_t>(
            -obj_index);

    if (from_end == 0u ||
        from_end > count) {
        return std::nullopt;
    }

    return
        count -
        static_cast<std::size_t>(
            from_end);
}

struct ObjCorner {
    std::size_t position{0};
    std::optional<std::size_t> uv{};
    std::optional<std::size_t> normal{};
};

bool parse_face_corner(
    std::string_view token,
    std::size_t position_count,
    std::size_t uv_count,
    std::size_t normal_count,
    ObjCorner& corner,
    std::string* error) {

    const auto first_slash =
        token.find('/');

    const auto second_slash =
        first_slash ==
            std::string_view::npos
            ? std::string_view::npos
            : token.find(
                '/',
                first_slash + 1u);

    const auto position_text =
        first_slash ==
            std::string_view::npos
            ? token
            : token.substr(
                0,
                first_slash);

    std::int64_t raw_position = 0;

    if (!parse_index(
            position_text,
            raw_position)) {

        set_error(
            error,
            "OBJ face position index is invalid");
        return false;
    }

    const auto position =
        resolve_index(
            raw_position,
            position_count);

    if (!position) {
        set_error(
            error,
            "OBJ face position index is out of range");
        return false;
    }

    corner = {};
    corner.position = *position;

    if (first_slash ==
        std::string_view::npos) {
        return true;
    }

    const auto uv_text =
        second_slash ==
            std::string_view::npos
            ? token.substr(
                first_slash + 1u)
            : token.substr(
                first_slash + 1u,
                second_slash -
                    first_slash -
                    1u);

    if (!uv_text.empty()) {
        std::int64_t raw_uv = 0;

        if (!parse_index(
                uv_text,
                raw_uv)) {

            set_error(
                error,
                "OBJ face texture index is invalid");
            return false;
        }

        const auto uv =
            resolve_index(
                raw_uv,
                uv_count);

        if (!uv) {
            set_error(
                error,
                "OBJ face texture index is out of range");
            return false;
        }

        corner.uv = *uv;
    }

    if (second_slash ==
        std::string_view::npos) {
        return true;
    }

    if (token.find(
            '/',
            second_slash + 1u) !=
        std::string_view::npos) {

        set_error(
            error,
            "OBJ face corner has too many slash-separated indices");
        return false;
    }

    const auto normal_text =
        token.substr(
            second_slash + 1u);

    if (!normal_text.empty()) {
        std::int64_t raw_normal = 0;

        if (!parse_index(
                normal_text,
                raw_normal)) {

            set_error(
                error,
                "OBJ face normal index is invalid");
            return false;
        }

        const auto normal =
            resolve_index(
                raw_normal,
                normal_count);

        if (!normal) {
            set_error(
                error,
                "OBJ face normal index is out of range");
            return false;
        }

        corner.normal = *normal;
    }

    return true;
}

core::Vec3 subtract(
    core::Vec3 left,
    core::Vec3 right) noexcept {

    return {
        left.x - right.x,
        left.y - right.y,
        left.z - right.z
    };
}

core::Vec3 cross(
    core::Vec3 a,
    core::Vec3 b) noexcept {

    return {
        a.y * b.z -
            a.z * b.y,
        a.z * b.x -
            a.x * b.z,
        a.x * b.y -
            a.y * b.x
    };
}

std::optional<core::Vec3>
normalized(
    core::Vec3 value) noexcept {

    const auto length_squared =
        value.x * value.x +
        value.y * value.y +
        value.z * value.z;

    if (!std::isfinite(
            length_squared) ||
        length_squared <= 1e-20f) {
        return std::nullopt;
    }

    const auto inverse =
        1.0f /
        std::sqrt(
            length_squared);

    return core::Vec3{
        value.x * inverse,
        value.y * inverse,
        value.z * inverse
    };
}

void calculate_bounds(
    MeshData& mesh) {

    if (mesh.vertices.empty()) {
        mesh.bounds = {};
        return;
    }

    auto minimum =
        mesh.vertices.front()
            .position;

    auto maximum =
        minimum;

    for (const auto& vertex :
         mesh.vertices) {

        minimum.x =
            std::min(
                minimum.x,
                vertex.position.x);

        minimum.y =
            std::min(
                minimum.y,
                vertex.position.y);

        minimum.z =
            std::min(
                minimum.z,
                vertex.position.z);

        maximum.x =
            std::max(
                maximum.x,
                vertex.position.x);

        maximum.y =
            std::max(
                maximum.y,
                vertex.position.y);

        maximum.z =
            std::max(
                maximum.z,
                vertex.position.z);
    }

    mesh.bounds.center = {
        (minimum.x + maximum.x) *
            0.5f,
        (minimum.y + maximum.y) *
            0.5f,
        (minimum.z + maximum.z) *
            0.5f
    };

    mesh.bounds.extents = {
        (maximum.x - minimum.x) *
            0.5f,
        (maximum.y - minimum.y) *
            0.5f,
        (maximum.z - minimum.z) *
            0.5f
    };
}

} // namespace

bool discover_obj_material_slots(
    const ResolvedModelAsset& asset,
    std::vector<ObjMaterialSlot>& materials,
    std::string* error) {

    materials.clear();

    if (!asset.guid.valid()) {
        set_error(
            error,
            "OBJ model AssetGuid is invalid");
        return false;
    }

    std::error_code filesystem_error;

    const auto file_bytes =
        std::filesystem::file_size(
            asset.source_path,
            filesystem_error);

    if (filesystem_error ||
        file_bytes == 0u ||
        file_bytes > kMaxObjBytes) {

        set_error(
            error,
            "OBJ source is unavailable, empty or exceeds 256 MiB");
        return false;
    }

    std::ifstream input(
        asset.source_path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open OBJ source");
        return false;
    }

    std::unordered_map<
        std::string,
        std::uint32_t>
        slots;

    std::string line;
    std::size_t line_number = 0u;

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

        const auto fields =
            tokens(view);

        if (fields.empty() ||
            fields.front() !=
                "usemtl") {
            continue;
        }

        const auto material_name =
            trim(
                view.substr(
                    fields.front()
                        .size()));

        if (material_name.empty()) {
            set_error(
                error,
                "OBJ usemtl name is missing at line " +
                    std::to_string(
                        line_number));
            return false;
        }

        const std::string key{
            material_name};

        if (slots.contains(
                key)) {
            continue;
        }

        if (slots.size() >=
            kMeshMaterialUnassigned) {

            set_error(
                error,
                "OBJ material slot range exceeds NEngine limit");
            return false;
        }

        const auto slot =
            static_cast<std::uint32_t>(
                slots.size());

        slots.emplace(
            key,
            slot);

        materials.push_back({
            slot,
            key
        });
    }

    if (!input.eof() &&
        input.fail()) {

        set_error(
            error,
            "OBJ source read failed during material discovery");
        return false;
    }

    return true;
}

bool decode_obj_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
    std::string* error) {

    mesh = {};

    if (!asset.guid.valid()) {
        set_error(
            error,
            "OBJ model AssetGuid is invalid");
        return false;
    }

    std::error_code filesystem_error;

    const auto file_bytes =
        std::filesystem::file_size(
            asset.source_path,
            filesystem_error);

    if (filesystem_error ||
        file_bytes == 0u ||
        file_bytes >
            kMaxObjBytes) {

        set_error(
            error,
            "OBJ source is unavailable, empty or exceeds 256 MiB");
        return false;
    }

    std::ifstream input(
        asset.source_path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open OBJ source");
        return false;
    }

    std::vector<core::Vec3>
        positions;

    std::vector<core::Vec2>
        texture_coordinates;

    std::vector<core::Vec3>
        normals;

    std::vector<ObjMaterialSlot>
        discovered_materials;

    if (!discover_obj_material_slots(
            asset,
            discovered_materials,
            error)) {
        return false;
    }

    std::unordered_map<
        std::string,
        std::uint32_t>
        material_slots;

    material_slots.reserve(
        discovered_materials.size());

    for (const auto& material :
         discovered_materials) {

        material_slots.emplace(
            material.name,
            material.slot);
    }

    std::uint32_t
        current_material_slot =
            kMeshMaterialUnassigned;

    std::string line;
    std::size_t line_number = 0u;

    while (std::getline(
               input,
               line)) {

        ++line_number;

        std::string_view view =
            trim(line);

        if (view.empty() ||
            view.front() == '#') {
            continue;
        }

        const auto fields =
            tokens(view);

        if (fields.empty()) {
            continue;
        }

        const auto directive =
            fields.front();

        if (directive == "v") {
            if (fields.size() <
                4u) {

                set_error(
                    error,
                    "OBJ vertex record is incomplete at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            core::Vec3 value;

            if (!parse_float(
                    fields[1],
                    value.x) ||
                !parse_float(
                    fields[2],
                    value.y) ||
                !parse_float(
                    fields[3],
                    value.z)) {

                set_error(
                    error,
                    "OBJ vertex coordinates are invalid at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            positions.push_back(
                value);

            continue;
        }

        if (directive == "vt") {
            if (fields.size() <
                3u) {

                set_error(
                    error,
                    "OBJ texture coordinate is incomplete at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            core::Vec2 value;

            if (!parse_float(
                    fields[1],
                    value.x) ||
                !parse_float(
                    fields[2],
                    value.y)) {

                set_error(
                    error,
                    "OBJ texture coordinates are invalid at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            // OBJ texture V is traditionally bottom-up while NEngine's
            // decoded image convention is top-left.
            value.y =
                1.0f -
                value.y;

            texture_coordinates.push_back(
                value);

            continue;
        }

        if (directive == "vn") {
            if (fields.size() <
                4u) {

                set_error(
                    error,
                    "OBJ normal record is incomplete at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            core::Vec3 value;

            if (!parse_float(
                    fields[1],
                    value.x) ||
                !parse_float(
                    fields[2],
                    value.y) ||
                !parse_float(
                    fields[3],
                    value.z)) {

                set_error(
                    error,
                    "OBJ normal coordinates are invalid at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            const auto unit =
                normalized(
                    value);

            if (!unit) {
                set_error(
                    error,
                    "OBJ normal has zero length at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            normals.push_back(
                *unit);

            continue;
        }

        if (directive == "usemtl") {
            const auto material_name =
                trim(
                    view.substr(
                        directive.size()));

            if (material_name.empty()) {
                set_error(
                    error,
                    "OBJ usemtl name is missing at line " +
                        std::to_string(
                            line_number));
                return false;
            }

            const auto existing =
                material_slots.find(
                    std::string{
                        material_name});

            if (existing ==
                material_slots.end()) {

                set_error(
                    error,
                    "OBJ usemtl discovery/decoder slot mismatch");
                return false;
            }

            current_material_slot =
                existing->second;

            continue;
        }

        if (directive != "f") {
            continue;
        }

        if (fields.size() <
            4u) {

            set_error(
                error,
                "OBJ face has fewer than three vertices at line " +
                    std::to_string(
                        line_number));
            return false;
        }

        if (mesh.submeshes.empty() ||
            mesh.submeshes.back()
                .material_slot !=
                    current_material_slot) {

            if (mesh.indices.size() >
                std::numeric_limits<
                    std::uint32_t>::max()) {

                set_error(
                    error,
                    "OBJ submesh index range exceeds NEngine limit");
                return false;
            }

            mesh.submeshes.push_back({
                static_cast<std::uint32_t>(
                    mesh.indices.size()),
                0u,
                current_material_slot
            });
        }

        std::vector<ObjCorner>
            corners;

        corners.reserve(
            fields.size() - 1u);

        for (std::size_t i = 1u;
             i < fields.size();
             ++i) {

            ObjCorner corner;

            if (!parse_face_corner(
                    fields[i],
                    positions.size(),
                    texture_coordinates.size(),
                    normals.size(),
                    corner,
                    error)) {

                if (error) {
                    *error +=
                        " at line " +
                        std::to_string(
                            line_number);
                }

                return false;
            }

            corners.push_back(
                corner);
        }

        for (std::size_t fan = 1u;
             fan + 1u <
                corners.size();
             ++fan) {

            const ObjCorner triangle[] = {
                corners[0],
                corners[fan],
                corners[fan + 1u]
            };

            if (mesh.vertices.size() >
                    std::numeric_limits<
                        std::uint32_t>::max() -
                        3u ||
                mesh.indices.size() >
                    std::numeric_limits<
                        std::uint32_t>::max() -
                        3u) {

                set_error(
                    error,
                    "OBJ mesh exceeds 32-bit NEngine mesh limits");
                return false;
            }

            MeshVertex output[3];

            for (std::size_t i = 0u;
                 i < 3u;
                 ++i) {

                auto position =
                    positions[
                        triangle[i]
                            .position];

                // Match the glTF import convention: source model data
                // enters NEngine left-handed space via Z reflection.
                position.z =
                    -position.z;

                output[i].position =
                    position;

                if (triangle[i].uv) {
                    output[i].uv =
                        texture_coordinates[
                            *triangle[i].uv];
                }

                if (triangle[i].normal) {
                    auto normal =
                        normals[
                            *triangle[i]
                                .normal];

                    normal.z =
                        -normal.z;

                    output[i].normal =
                        normal;
                }
            }

            // Reflection reverses handedness. Emit source (a,c,b).
            std::swap(
                output[1],
                output[2]);

            const bool missing_normal =
                !triangle[0].normal ||
                !triangle[1].normal ||
                !triangle[2].normal;

            if (missing_normal) {
                const auto face =
                    normalized(
                        cross(
                            subtract(
                                output[1]
                                    .position,
                                output[0]
                                    .position),
                            subtract(
                                output[2]
                                    .position,
                                output[0]
                                    .position)));

                if (!face) {
                    set_error(
                        error,
                        "OBJ face is degenerate at line " +
                            std::to_string(
                                line_number));
                    return false;
                }

                for (auto& vertex :
                     output) {
                    vertex.normal =
                        *face;
                }
            }

            const auto base =
                static_cast<std::uint32_t>(
                    mesh.vertices.size());

            mesh.vertices.insert(
                mesh.vertices.end(),
                std::begin(output),
                std::end(output));

            mesh.indices.insert(
                mesh.indices.end(),
                {
                    base + 0u,
                    base + 1u,
                    base + 2u
                });

            mesh.submeshes.back()
                .index_count += 3u;
        }
    }

    if (!input.eof() &&
        input.fail()) {

        set_error(
            error,
            "OBJ source read failed");
        return false;
    }

    if (mesh.vertices.empty() ||
        mesh.indices.empty()) {

        set_error(
            error,
            "OBJ source contains no renderable faces");
        return false;
    }

    calculate_bounds(
        mesh);

    if (!mesh.valid()) {
        set_error(
            error,
            "decoded OBJ MeshData is invalid");
        return false;
    }

    return true;
}

} // namespace nengine::render
