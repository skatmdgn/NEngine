#include "nengine/render/gltf_mesh.hpp"

#include "stb_image.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

struct JsonValue {
    enum class Kind {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object,
    };

    Kind kind{Kind::Null};
    bool boolean{false};
    double number{0.0};
    std::string string{};
    std::vector<JsonValue> array{};
    std::unordered_map<std::string, JsonValue> object{};
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text)
        : text_(text) {}

    bool parse(
        JsonValue& value,
        std::string& error) {

        skip_space();

        if (!parse_value(value, error)) {
            return false;
        }

        skip_space();

        if (position_ != text_.size()) {
            error = "unexpected trailing JSON content";
            return false;
        }

        return true;
    }

private:
    void skip_space() noexcept {
        while (position_ < text_.size()) {
            const char ch =
                text_[position_];

            if (ch != ' ' &&
                ch != '\t' &&
                ch != '\r' &&
                ch != '\n') {
                break;
            }

            ++position_;
        }
    }

    bool consume(char expected) noexcept {
        if (position_ >= text_.size() ||
            text_[position_] != expected) {
            return false;
        }

        ++position_;
        return true;
    }

    bool consume_literal(
        std::string_view literal) noexcept {

        if (text_.substr(
                position_,
                literal.size()) !=
            literal) {
            return false;
        }

        position_ += literal.size();
        return true;
    }

    bool parse_value(
        JsonValue& value,
        std::string& error) {

        skip_space();

        if (position_ >= text_.size()) {
            error = "unexpected end of JSON";
            return false;
        }

        const char ch =
            text_[position_];

        if (ch == '{') {
            return parse_object(
                value,
                error);
        }

        if (ch == '[') {
            return parse_array(
                value,
                error);
        }

        if (ch == '"') {
            value = {};
            value.kind =
                JsonValue::Kind::String;

            return parse_string(
                value.string,
                error);
        }

        if (ch == 't') {
            if (!consume_literal("true")) {
                error = "invalid JSON literal";
                return false;
            }

            value = {};
            value.kind =
                JsonValue::Kind::Boolean;
            value.boolean = true;
            return true;
        }

        if (ch == 'f') {
            if (!consume_literal("false")) {
                error = "invalid JSON literal";
                return false;
            }

            value = {};
            value.kind =
                JsonValue::Kind::Boolean;
            value.boolean = false;
            return true;
        }

        if (ch == 'n') {
            if (!consume_literal("null")) {
                error = "invalid JSON literal";
                return false;
            }

            value = {};
            return true;
        }

        return parse_number(
            value,
            error);
    }

    bool parse_object(
        JsonValue& value,
        std::string& error) {

        if (!consume('{')) {
            error = "expected JSON object";
            return false;
        }

        value = {};
        value.kind =
            JsonValue::Kind::Object;

        skip_space();

        if (consume('}')) {
            return true;
        }

        while (position_ < text_.size()) {
            skip_space();

            std::string key;

            if (!parse_string(
                    key,
                    error)) {
                return false;
            }

            skip_space();

            if (!consume(':')) {
                error =
                    "expected ':' in JSON object";
                return false;
            }

            JsonValue child;

            if (!parse_value(
                    child,
                    error)) {
                return false;
            }

            value.object.emplace(
                std::move(key),
                std::move(child));

            skip_space();

            if (consume('}')) {
                return true;
            }

            if (!consume(',')) {
                error =
                    "expected ',' in JSON object";
                return false;
            }
        }

        error =
            "unterminated JSON object";
        return false;
    }

    bool parse_array(
        JsonValue& value,
        std::string& error) {

        if (!consume('[')) {
            error = "expected JSON array";
            return false;
        }

        value = {};
        value.kind =
            JsonValue::Kind::Array;

        skip_space();

        if (consume(']')) {
            return true;
        }

        while (position_ < text_.size()) {
            JsonValue child;

            if (!parse_value(
                    child,
                    error)) {
                return false;
            }

            value.array.push_back(
                std::move(child));

            skip_space();

            if (consume(']')) {
                return true;
            }

            if (!consume(',')) {
                error =
                    "expected ',' in JSON array";
                return false;
            }
        }

        error =
            "unterminated JSON array";
        return false;
    }

    static bool append_utf8(
        std::uint32_t codepoint,
        std::string& output) {

        if (codepoint <= 0x7Fu) {
            output.push_back(
                static_cast<char>(
                    codepoint));
            return true;
        }

        if (codepoint <= 0x7FFu) {
            output.push_back(
                static_cast<char>(
                    0xC0u |
                    (codepoint >> 6u)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    (codepoint & 0x3Fu)));

            return true;
        }

        if (codepoint <= 0xFFFFu) {
            output.push_back(
                static_cast<char>(
                    0xE0u |
                    (codepoint >> 12u)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    ((codepoint >> 6u) &
                     0x3Fu)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    (codepoint & 0x3Fu)));

            return true;
        }

        if (codepoint <= 0x10FFFFu) {
            output.push_back(
                static_cast<char>(
                    0xF0u |
                    (codepoint >> 18u)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    ((codepoint >> 12u) &
                     0x3Fu)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    ((codepoint >> 6u) &
                     0x3Fu)));

            output.push_back(
                static_cast<char>(
                    0x80u |
                    (codepoint & 0x3Fu)));

            return true;
        }

        return false;
    }

    static int hex_value(char ch) noexcept {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }

        if (ch >= 'a' && ch <= 'f') {
            return 10 + ch - 'a';
        }

        if (ch >= 'A' && ch <= 'F') {
            return 10 + ch - 'A';
        }

        return -1;
    }

    bool parse_string(
        std::string& output,
        std::string& error) {

        if (!consume('"')) {
            error =
                "expected JSON string";
            return false;
        }

        output.clear();

        while (position_ < text_.size()) {
            const char ch =
                text_[position_++];

            if (ch == '"') {
                return true;
            }

            if (static_cast<
                    unsigned char>(
                        ch) < 0x20u) {

                error =
                    "invalid control character in JSON string";
                return false;
            }

            if (ch != '\\') {
                output.push_back(ch);
                continue;
            }

            if (position_ >= text_.size()) {
                error =
                    "unterminated JSON escape";
                return false;
            }

            const char escaped =
                text_[position_++];

            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                output.push_back(
                    escaped);
                break;

            case 'b':
                output.push_back('\b');
                break;

            case 'f':
                output.push_back('\f');
                break;

            case 'n':
                output.push_back('\n');
                break;

            case 'r':
                output.push_back('\r');
                break;

            case 't':
                output.push_back('\t');
                break;

            case 'u': {
                if (position_ + 4u >
                    text_.size()) {

                    error =
                        "truncated JSON unicode escape";
                    return false;
                }

                std::uint32_t codepoint = 0;

                for (int i = 0; i < 4; ++i) {
                    const int value =
                        hex_value(
                            text_[
                                position_ +
                                static_cast<
                                    std::size_t>(
                                        i)]);

                    if (value < 0) {
                        error =
                            "invalid JSON unicode escape";
                        return false;
                    }

                    codepoint =
                        (codepoint << 4u) |
                        static_cast<
                            std::uint32_t>(
                                value);
                }

                position_ += 4u;

                if (!append_utf8(
                        codepoint,
                        output)) {

                    error =
                        "invalid JSON unicode codepoint";
                    return false;
                }

                break;
            }

            default:
                error =
                    "unsupported JSON escape";
                return false;
            }
        }

        error =
            "unterminated JSON string";
        return false;
    }

    bool parse_number(
        JsonValue& value,
        std::string& error) {

        const auto begin =
            position_;

        if (position_ < text_.size() &&
            text_[position_] == '-') {
            ++position_;
        }

        if (position_ >= text_.size()) {
            error =
                "invalid JSON number";
            return false;
        }

        if (text_[position_] == '0') {
            ++position_;
        } else {
            if (text_[position_] < '1' ||
                text_[position_] > '9') {

                error =
                    "invalid JSON number";
                return false;
            }

            while (position_ < text_.size() &&
                   text_[position_] >= '0' &&
                   text_[position_] <= '9') {
                ++position_;
            }
        }

        if (position_ < text_.size() &&
            text_[position_] == '.') {

            ++position_;

            const auto fraction_begin =
                position_;

            while (position_ < text_.size() &&
                   text_[position_] >= '0' &&
                   text_[position_] <= '9') {
                ++position_;
            }

            if (fraction_begin == position_) {
                error =
                    "invalid JSON fraction";
                return false;
            }
        }

        if (position_ < text_.size() &&
            (text_[position_] == 'e' ||
             text_[position_] == 'E')) {

            ++position_;

            if (position_ < text_.size() &&
                (text_[position_] == '+' ||
                 text_[position_] == '-')) {
                ++position_;
            }

            const auto exponent_begin =
                position_;

            while (position_ < text_.size() &&
                   text_[position_] >= '0' &&
                   text_[position_] <= '9') {
                ++position_;
            }

            if (exponent_begin == position_) {
                error =
                    "invalid JSON exponent";
                return false;
            }
        }

        const std::string token{
            text_.substr(
                begin,
                position_ - begin)};

        char* end = nullptr;

        const double number =
            std::strtod(
                token.c_str(),
                &end);

        if (!end ||
            end != token.c_str() +
                token.size() ||
            !std::isfinite(number)) {

            error =
                "invalid JSON number";
            return false;
        }

        value = {};
        value.kind =
            JsonValue::Kind::Number;
        value.number = number;
        return true;
    }

    std::string_view text_{};
    std::size_t position_{0};
};

const JsonValue* member(
    const JsonValue& object,
    std::string_view key) noexcept {

    if (object.kind !=
        JsonValue::Kind::Object) {
        return nullptr;
    }

    const auto it =
        object.object.find(
            std::string{key});

    return it == object.object.end()
        ? nullptr
        : &it->second;
}

bool unsigned_value(
    const JsonValue* value,
    std::uint64_t& output) noexcept {

    if (!value ||
        value->kind !=
            JsonValue::Kind::Number ||
        value->number < 0.0 ||
        value->number >
            static_cast<double>(
                std::numeric_limits<
                    std::uint64_t>::max()) ||
        std::floor(value->number) !=
            value->number) {

        return false;
    }

    output =
        static_cast<std::uint64_t>(
            value->number);

    return true;
}

std::optional<std::size_t>
index_value(
    const JsonValue* value) noexcept {

    std::uint64_t number = 0;

    if (!unsigned_value(
            value,
            number) ||
        number >
            static_cast<std::uint64_t>(
                std::numeric_limits<
                    std::size_t>::max())) {

        return std::nullopt;
    }

    return static_cast<std::size_t>(
        number);
}

std::optional<std::string>
string_value(
    const JsonValue* value) {

    if (!value ||
        value->kind !=
            JsonValue::Kind::String) {
        return std::nullopt;
    }

    return value->string;
}

std::uint32_t read_u32_le(
    const std::uint8_t* bytes) noexcept {

    return
        static_cast<std::uint32_t>(
            bytes[0]) |
        (static_cast<std::uint32_t>(
            bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(
            bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(
            bytes[3]) << 24u);
}

std::uint16_t read_u16_le(
    const std::uint8_t* bytes) noexcept {

    return
        static_cast<std::uint16_t>(
            bytes[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                bytes[1]) << 8u);
}

float read_f32_le(
    const std::uint8_t* bytes) noexcept {

    return std::bit_cast<float>(
        read_u32_le(bytes));
}

bool read_binary_file(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary |
            std::ios::ate);

    if (!input) {
        set_error(
            error,
            "could not open glTF buffer: " +
                path.generic_string());
        return false;
    }

    const auto size =
        input.tellg();

    if (size < 0) {
        set_error(
            error,
            "could not determine glTF buffer size");
        return false;
    }

    bytes.resize(
        static_cast<std::size_t>(
            size));

    input.seekg(0);

    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(
                bytes.data()),
            static_cast<
                std::streamsize>(
                    bytes.size()));
    }

    if (!input && !bytes.empty()) {
        set_error(
            error,
            "could not read glTF buffer");
        return false;
    }

    return true;
}

std::optional<std::filesystem::path> gltf_sidecar_path(
    const std::filesystem::path& model_path,
    std::string_view uri,
    std::string* error) {

    // The current importer supports only literal relative file paths.
    // Block traversal and URI schemes, even on a non-Windows build.
    const std::filesystem::path relative{
        std::string{uri}};

    if (relative.empty() ||
        relative.is_absolute() ||
        relative.has_root_path() ||
        uri.find(':') != std::string_view::npos ||
        uri.find('\\') != std::string_view::npos ||
        uri.find('%') != std::string_view::npos ||
        uri.find('?') != std::string_view::npos ||
        uri.find('#') != std::string_view::npos) {

        set_error(error, "unsafe or unsupported external glTF URI");
        return std::nullopt;
    }

    for (const auto& segment : relative) {
        if (segment == ".." || segment == ".") {
            set_error(error, "glTF URI must remain below model directory");
            return std::nullopt;
        }
    }

    return model_path.parent_path() / relative;
}

int base64_value(
    unsigned char ch) noexcept {

    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }

    if (ch >= 'a' && ch <= 'z') {
        return 26 + ch - 'a';
    }

    if (ch >= '0' && ch <= '9') {
        return 52 + ch - '0';
    }

    if (ch == '+') {
        return 62;
    }

    if (ch == '/') {
        return 63;
    }

    return -1;
}

bool decode_base64(
    std::string_view encoded,
    std::vector<std::uint8_t>& output,
    std::string* error) {

    output.clear();

    std::uint32_t accumulator = 0;
    int bits = 0;
    bool saw_padding = false;

    for (const unsigned char ch :
         encoded) {

        if (ch == ' ' ||
            ch == '\t' ||
            ch == '\r' ||
            ch == '\n') {
            continue;
        }

        if (ch == '=') {
            saw_padding = true;
            continue;
        }

        if (saw_padding) {
            set_error(
                error,
                "invalid base64 padding in glTF data URI");
            return false;
        }

        const int value =
            base64_value(ch);

        if (value < 0) {
            set_error(
                error,
                "invalid base64 character in glTF data URI");
            return false;
        }

        accumulator =
            (accumulator << 6u) |
            static_cast<std::uint32_t>(
                value);

        bits += 6;

        if (bits >= 8) {
            bits -= 8;

            output.push_back(
                static_cast<std::uint8_t>(
                    (accumulator >>
                        static_cast<
                            std::uint32_t>(
                                bits)) &
                    0xFFu));
        }
    }

    return true;
}

struct GltfSource {
    std::string json{};
    std::vector<std::uint8_t> glb_bin{};
    bool glb{false};
};

bool load_glb(
    const std::filesystem::path& path,
    GltfSource& source,
    std::string* error) {

    std::vector<std::uint8_t> bytes;

    if (!read_binary_file(
            path,
            bytes,
            error)) {
        return false;
    }

    if (bytes.size() < 12u ||
        read_u32_le(
            bytes.data()) !=
            0x46546C67u ||
        read_u32_le(
            bytes.data() + 4u) !=
            2u) {

        set_error(
            error,
            "invalid or unsupported GLB 2.0 header");
        return false;
    }

    const auto declared_length =
        read_u32_le(
            bytes.data() + 8u);

    if (declared_length !=
        bytes.size()) {

        set_error(
            error,
            "GLB declared length does not match file size");
        return false;
    }

    bool saw_json = false;

    std::size_t offset = 12u;

    while (offset + 8u <=
           bytes.size()) {

        const auto chunk_length =
            read_u32_le(
                bytes.data() +
                offset);

        const auto chunk_type =
            read_u32_le(
                bytes.data() +
                offset +
                4u);

        offset += 8u;

        if (offset +
                static_cast<std::size_t>(
                    chunk_length) >
            bytes.size()) {

            set_error(
                error,
                "GLB chunk exceeds file size");
            return false;
        }

        if (chunk_type ==
            0x4E4F534Au) {

            if (saw_json) {
                set_error(
                    error,
                    "GLB contains multiple JSON chunks");
                return false;
            }

            source.json.assign(
                reinterpret_cast<
                    const char*>(
                        bytes.data() +
                        offset),
                chunk_length);

            while (!source.json.empty() &&
                   (source.json.back() == '\0' ||
                    source.json.back() == ' ')) {
                source.json.pop_back();
            }

            saw_json = true;
        } else if (
            chunk_type ==
                0x004E4942u &&
            source.glb_bin.empty()) {

            source.glb_bin.assign(
                bytes.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            offset),
                bytes.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            offset +
                            chunk_length));
        }

        offset +=
            static_cast<std::size_t>(
                chunk_length);
    }

    if (!saw_json) {
        set_error(
            error,
            "GLB JSON chunk is missing");
        return false;
    }

    source.glb = true;
    return true;
}

bool load_gltf_source(
    const ResolvedModelAsset& asset,
    GltfSource& source,
    std::string* error) {

    source = {};

    std::string format =
        asset.metadata.format;

    std::transform(
        format.begin(),
        format.end(),
        format.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    if (!format.empty() &&
        format.front() == '.') {
        format.erase(
            format.begin());
    }

    if (format == "glb" ||
        asset.source_path
            .extension() == ".glb") {

        return load_glb(
            asset.source_path,
            source,
            error);
    }

    if (format != "gltf" &&
        asset.source_path
            .extension() != ".gltf") {

        set_error(
            error,
            "model asset is not glTF/GLB");
        return false;
    }

    std::ifstream input(
        asset.source_path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open glTF JSON source");
        return false;
    }

    source.json.assign(
        std::istreambuf_iterator<char>{
            input},
        std::istreambuf_iterator<char>{});

    if (!input.good() &&
        !input.eof()) {

        set_error(
            error,
            "could not read glTF JSON source");
        return false;
    }

    return true;
}

struct BufferView {
    std::size_t buffer{0};
    std::size_t offset{0};
    std::size_t length{0};
    std::size_t stride{0};
};

struct Accessor {
    std::size_t view{0};
    std::size_t offset{0};
    std::size_t count{0};
    std::uint32_t component_type{0};
    std::string type{};
};

bool parse_buffer_views(
    const JsonValue& root,
    std::vector<BufferView>& views,
    std::string* error) {

    const auto* json_views =
        member(
            root,
            "bufferViews");

    if (!json_views) {
        views.clear();
        return true;
    }

    if (json_views->kind !=
        JsonValue::Kind::Array) {

        set_error(
            error,
            "glTF bufferViews must be an array");
        return false;
    }

    views.clear();
    views.reserve(
        json_views->array.size());

    for (const auto& item :
         json_views->array) {

        const auto buffer =
            index_value(
                member(
                    item,
                    "buffer"));

        const auto length =
            index_value(
                member(
                    item,
                    "byteLength"));

        if (!buffer ||
            !length) {

            set_error(
                error,
                "glTF bufferView requires buffer and byteLength");
            return false;
        }

        BufferView view;
        view.buffer = *buffer;
        view.length = *length;

        if (const auto offset =
                index_value(
                    member(
                        item,
                        "byteOffset"))) {
            view.offset = *offset;
        }

        if (const auto stride =
                index_value(
                    member(
                        item,
                        "byteStride"))) {
            view.stride = *stride;
        }

        views.push_back(
            view);
    }

    return true;
}

bool parse_accessors(
    const JsonValue& root,
    std::vector<Accessor>& accessors,
    std::string* error) {

    const auto* json_accessors =
        member(
            root,
            "accessors");

    if (!json_accessors ||
        json_accessors->kind !=
            JsonValue::Kind::Array) {

        set_error(
            error,
            "glTF accessors array is missing");
        return false;
    }

    accessors.clear();
    accessors.reserve(
        json_accessors->array.size());

    for (const auto& item :
         json_accessors->array) {

        if (member(
                item,
                "sparse")) {

            set_error(
                error,
                "sparse glTF accessors are not supported yet");
            return false;
        }

        const auto view =
            index_value(
                member(
                    item,
                    "bufferView"));

        const auto count =
            index_value(
                member(
                    item,
                    "count"));

        std::uint64_t component = 0;

        const auto type =
            string_value(
                member(
                    item,
                    "type"));

        if (!view ||
            !count ||
            !unsigned_value(
                member(
                    item,
                    "componentType"),
                component) ||
            component >
                std::numeric_limits<
                    std::uint32_t>::max() ||
            !type) {

            set_error(
                error,
                "glTF accessor is missing required fields");
            return false;
        }

        Accessor accessor;
        accessor.view = *view;
        accessor.count = *count;
        accessor.component_type =
            static_cast<std::uint32_t>(
                component);
        accessor.type = *type;

        if (const auto offset =
                index_value(
                    member(
                        item,
                        "byteOffset"))) {
            accessor.offset =
                *offset;
        }

        accessors.push_back(
            std::move(accessor));
    }

    return true;
}

bool load_buffers(
    const JsonValue& root,
    const GltfSource& source,
    const std::filesystem::path& model_path,
    std::vector<
        std::vector<std::uint8_t>>& buffers,
    std::string* error) {

    const auto* json_buffers =
        member(
            root,
            "buffers");

    if (!json_buffers ||
        json_buffers->kind !=
            JsonValue::Kind::Array ||
        json_buffers->array.empty()) {

        set_error(
            error,
            "glTF buffers array is missing");
        return false;
    }

    buffers.clear();
    buffers.reserve(
        json_buffers->array.size());

    for (std::size_t i = 0;
         i < json_buffers->array.size();
         ++i) {

        const auto& item =
            json_buffers->array[i];

        const auto expected_length =
            index_value(
                member(
                    item,
                    "byteLength"));

        if (!expected_length) {
            set_error(
                error,
                "glTF buffer byteLength is missing");
            return false;
        }

        std::vector<std::uint8_t> bytes;

        const auto uri =
            string_value(
                member(
                    item,
                    "uri"));

        if (!uri) {
            if (!source.glb ||
                i != 0u ||
                source.glb_bin.empty()) {

                set_error(
                    error,
                    "glTF buffer without URI requires GLB BIN chunk");
                return false;
            }

            bytes =
                source.glb_bin;
        } else if (
            uri->rfind(
                "data:",
                0) == 0u) {

            const auto comma =
                uri->find(',');

            if (comma ==
                    std::string::npos ||
                uri->substr(
                    0,
                    comma)
                    .find(";base64") ==
                    std::string::npos) {

                set_error(
                    error,
                    "only base64 glTF data URIs are supported");
                return false;
            }

            if (!decode_base64(
                    std::string_view{
                        *uri}.substr(
                            comma + 1u),
                    bytes,
                    error)) {
                return false;
            }
        } else {
            const auto path = gltf_sidecar_path(
                model_path,
                *uri,
                error);

            if (!path ||
                !read_binary_file(
                    *path,
                    bytes,
                    error)) {
                return false;
            }
        }

        if (bytes.size() <
            *expected_length) {

            set_error(
                error,
                "glTF buffer payload is shorter than byteLength");
            return false;
        }

        buffers.push_back(
            std::move(bytes));
    }

    return true;
}

std::size_t component_size(
    std::uint32_t type) noexcept {

    switch (type) {
    case 5121u:
        return 1u;
    case 5123u:
        return 2u;
    case 5125u:
    case 5126u:
        return 4u;
    default:
        return 0u;
    }
}

std::size_t component_count(
    std::string_view type) noexcept {

    if (type == "SCALAR") {
        return 1u;
    }

    if (type == "VEC2") {
        return 2u;
    }

    if (type == "VEC3") {
        return 3u;
    }

    if (type == "VEC4") {
        return 4u;
    }

    return 0u;
}

bool accessor_span(
    const Accessor& accessor,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    const std::uint8_t*& data,
    std::size_t& stride,
    std::size_t expected_components,
    std::uint32_t expected_component_type,
    std::string* error) {

    if (accessor.view >=
        views.size()) {

        set_error(
            error,
            "glTF accessor references invalid bufferView");
        return false;
    }

    if (accessor.component_type !=
            expected_component_type ||
        component_count(
            accessor.type) !=
            expected_components) {

        set_error(
            error,
            "glTF accessor type/componentType is unsupported for requested attribute");
        return false;
    }

    const auto& view =
        views[accessor.view];

    if (view.buffer >=
        buffers.size()) {

        set_error(
            error,
            "glTF bufferView references invalid buffer");
        return false;
    }

    const auto element_size =
        component_size(
            accessor.component_type) *
        expected_components;

    stride =
        view.stride != 0u
            ? view.stride
            : element_size;

    if (stride < element_size) {
        set_error(
            error,
            "glTF bufferView byteStride is smaller than accessor element");
        return false;
    }

    const auto& buffer =
        buffers[view.buffer];

    if (view.offset >
            buffer.size() ||
        view.length >
            buffer.size() -
                view.offset ||
        accessor.offset >
            view.length) {

        set_error(
            error,
            "glTF accessor range is outside bufferView");
        return false;
    }

    const auto start =
        view.offset +
        accessor.offset;

    if (accessor.count != 0u) {
        const auto required =
            static_cast<std::uint64_t>(
                accessor.count - 1u) *
                stride +
            element_size;

        if (required >
                view.length -
                    accessor.offset ||
            required >
                buffer.size() -
                    start) {

            set_error(
                error,
                "glTF accessor payload is truncated");
            return false;
        }
    }

    data =
        buffer.data() +
        start;

    return true;
}

bool read_vec3_accessor(
    std::size_t accessor_index,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    std::vector<core::Vec3>& values,
    std::string* error) {

    if (accessor_index >=
        accessors.size()) {

        set_error(
            error,
            "glTF VEC3 accessor index is invalid");
        return false;
    }

    const auto& accessor =
        accessors[
            accessor_index];

    const std::uint8_t* data =
        nullptr;

    std::size_t stride = 0;

    if (!accessor_span(
            accessor,
            views,
            buffers,
            data,
            stride,
            3u,
            5126u,
            error)) {
        return false;
    }

    values.resize(
        accessor.count);

    for (std::size_t i = 0;
         i < accessor.count;
         ++i) {

        const auto* element =
            data + i * stride;

        values[i] = {
            read_f32_le(
                element + 0u),
            read_f32_le(
                element + 4u),
            read_f32_le(
                element + 8u)
        };
    }

    return true;
}

bool read_vec2_accessor(
    std::size_t accessor_index,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    std::vector<core::Vec2>& values,
    std::string* error) {

    if (accessor_index >=
        accessors.size()) {

        set_error(
            error,
            "glTF VEC2 accessor index is invalid");
        return false;
    }

    const auto& accessor =
        accessors[
            accessor_index];

    const std::uint8_t* data =
        nullptr;

    std::size_t stride = 0;

    if (!accessor_span(
            accessor,
            views,
            buffers,
            data,
            stride,
            2u,
            5126u,
            error)) {
        return false;
    }

    values.resize(
        accessor.count);

    for (std::size_t i = 0;
         i < accessor.count;
         ++i) {

        const auto* element =
            data + i * stride;

        values[i] = {
            read_f32_le(
                element + 0u),
            read_f32_le(
                element + 4u)
        };
    }

    return true;
}

bool read_index_accessor(
    std::size_t accessor_index,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    std::vector<std::uint32_t>& values,
    std::string* error) {

    if (accessor_index >=
        accessors.size()) {

        set_error(
            error,
            "glTF index accessor is invalid");
        return false;
    }

    const auto& accessor =
        accessors[
            accessor_index];

    if (accessor.type !=
        "SCALAR") {

        set_error(
            error,
            "glTF indices accessor must be SCALAR");
        return false;
    }

    const auto element_size =
        component_size(
            accessor.component_type);

    if (accessor.component_type !=
            5121u &&
        accessor.component_type !=
            5123u &&
        accessor.component_type !=
            5125u) {

        set_error(
            error,
            "glTF indices must use unsigned byte/short/int");
        return false;
    }

    if (accessor.view >=
        views.size()) {

        set_error(
            error,
            "glTF indices reference invalid bufferView");
        return false;
    }

    const auto& view =
        views[accessor.view];

    if (view.buffer >=
        buffers.size()) {

        set_error(
            error,
            "glTF indices bufferView references invalid buffer");
        return false;
    }

    const auto stride =
        view.stride != 0u
            ? view.stride
            : element_size;

    if (stride < element_size) {
        set_error(
            error,
            "glTF index byteStride is invalid");
        return false;
    }

    const auto& buffer =
        buffers[view.buffer];

    if (view.offset >
            buffer.size() ||
        view.length >
            buffer.size() -
                view.offset ||
        accessor.offset >
            view.length) {

        set_error(
            error,
            "glTF index accessor range is invalid");
        return false;
    }

    const auto start =
        view.offset +
        accessor.offset;

    if (accessor.count != 0u) {
        const auto required =
            static_cast<std::uint64_t>(
                accessor.count - 1u) *
                stride +
            element_size;

        if (required >
                view.length -
                    accessor.offset ||
            required >
                buffer.size() -
                    start) {

            set_error(
                error,
                "glTF index payload is truncated");
            return false;
        }
    }

    values.resize(
        accessor.count);

    for (std::size_t i = 0;
         i < accessor.count;
         ++i) {

        const auto* element =
            buffer.data() +
            start +
            i * stride;

        switch (accessor.component_type) {
        case 5121u:
            values[i] =
                element[0];
            break;

        case 5123u:
            values[i] =
                read_u16_le(
                    element);
            break;

        case 5125u:
            values[i] =
                read_u32_le(
                    element);
            break;

        default:
            return false;
        }
    }

    return true;
}

bool append_primitive(
    const JsonValue& primitive,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    MeshData& mesh,
    std::string* error) {

    if (const auto mode =
            index_value(
                member(
                    primitive,
                    "mode"));
        mode &&
        *mode != 4u) {

        set_error(
            error,
            "glTF decoder currently supports TRIANGLES primitives only");
        return false;
    }

    const auto* attributes =
        member(
            primitive,
            "attributes");

    if (!attributes ||
        attributes->kind !=
            JsonValue::Kind::Object) {

        set_error(
            error,
            "glTF primitive attributes object is missing");
        return false;
    }

    const auto position_accessor =
        index_value(
            member(
                *attributes,
                "POSITION"));

    if (!position_accessor) {
        set_error(
            error,
            "glTF primitive POSITION accessor is missing");
        return false;
    }

    std::vector<core::Vec3>
        positions;

    if (!read_vec3_accessor(
            *position_accessor,
            accessors,
            views,
            buffers,
            positions,
            error)) {
        return false;
    }

    std::vector<core::Vec3>
        normals;

    if (const auto normal_accessor =
            index_value(
                member(
                    *attributes,
                    "NORMAL"))) {

        if (!read_vec3_accessor(
                *normal_accessor,
                accessors,
                views,
                buffers,
                normals,
                error)) {
            return false;
        }

        if (normals.size() !=
            positions.size()) {

            set_error(
                error,
                "glTF NORMAL count does not match POSITION count");
            return false;
        }
    }

    std::vector<core::Vec2>
        uvs;

    if (const auto uv_accessor =
            index_value(
                member(
                    *attributes,
                    "TEXCOORD_0"))) {

        if (!read_vec2_accessor(
                *uv_accessor,
                accessors,
                views,
                buffers,
                uvs,
                error)) {
            return false;
        }

        if (uvs.size() !=
            positions.size()) {

            set_error(
                error,
                "glTF TEXCOORD_0 count does not match POSITION count");
            return false;
        }
    }

    if (positions.empty()) {
        set_error(
            error,
            "glTF primitive has no vertices");
        return false;
    }

    if (mesh.vertices.size() >
        std::numeric_limits<
            std::uint32_t>::max() -
            positions.size()) {

        set_error(
            error,
            "glTF mesh exceeds 32-bit vertex index range");
        return false;
    }

    const auto base =
        static_cast<std::uint32_t>(
            mesh.vertices.size());

    mesh.vertices.reserve(
        mesh.vertices.size() +
        positions.size());

    for (std::size_t i = 0;
         i < positions.size();
         ++i) {

        auto position =
            positions[i];

        // glTF uses a right-handed coordinate system while NEngine's
        // current renderer/camera convention is left-handed. Reflect Z
        // here so imported geometry enters the engine in native space.
        position.z =
            -position.z;

        auto normal =
            i < normals.size()
                ? normals[i]
                : core::Vec3{
                    0.0f,
                    0.0f,
                    1.0f};

        if (i < normals.size()) {
            normal.z =
                -normal.z;
        }

        mesh.vertices.push_back({
            position,
            normal,
            i < uvs.size()
                ? uvs[i]
                : core::Vec2{}
        });
    }

    std::vector<std::uint32_t>
        indices;

    if (const auto index_accessor =
            index_value(
                member(
                    primitive,
                    "indices"))) {

        if (!read_index_accessor(
                *index_accessor,
                accessors,
                views,
                buffers,
                indices,
                error)) {
            return false;
        }
    } else {
        indices.resize(
            positions.size());

        for (std::size_t i = 0;
             i < positions.size();
             ++i) {

            indices[i] =
                static_cast<
                    std::uint32_t>(
                        i);
        }
    }

    if (indices.empty() ||
        (indices.size() % 3u) != 0u) {

        set_error(
            error,
            "glTF triangle primitive index count is invalid");
        return false;
    }

    mesh.indices.reserve(
        mesh.indices.size() +
        indices.size());

    for (const auto index :
         indices) {

        if (index >=
            positions.size()) {

            set_error(
                error,
                "glTF primitive index references missing vertex");
            return false;
        }
    }

    // Reflecting one axis changes handedness and reverses winding.
    // Reverse each triangle so the existing Vulkan back-face culling
    // remains correct after the Z reflection.
    for (std::size_t i = 0;
         i < indices.size();
         i += 3u) {

        mesh.indices.push_back(
            base + indices[i + 0u]);

        mesh.indices.push_back(
            base + indices[i + 2u]);

        mesh.indices.push_back(
            base + indices[i + 1u]);
    }

    return true;
}

bool append_meshes(
    const JsonValue& root,
    const std::vector<Accessor>& accessors,
    const std::vector<BufferView>& views,
    const std::vector<
        std::vector<std::uint8_t>>& buffers,
    MeshData& mesh,
    std::string* error) {

    const auto* meshes =
        member(
            root,
            "meshes");

    if (!meshes ||
        meshes->kind !=
            JsonValue::Kind::Array ||
        meshes->array.empty()) {

        set_error(
            error,
            "glTF contains no meshes");
        return false;
    }

    for (const auto& mesh_value :
         meshes->array) {

        const auto* primitives =
            member(
                mesh_value,
                "primitives");

        if (!primitives ||
            primitives->kind !=
                JsonValue::Kind::Array ||
            primitives->array.empty()) {

            set_error(
                error,
                "glTF mesh contains no primitives");
            return false;
        }

        for (const auto& primitive :
             primitives->array) {

            if (!append_primitive(
                    primitive,
                    accessors,
                    views,
                    buffers,
                    mesh,
                    error)) {
                return false;
            }
        }
    }

    return true;
}

void calculate_bounds(
    MeshData& mesh) {

    if (mesh.vertices.empty()) {
        mesh.bounds = {};
        return;
    }

    core::Vec3 minimum =
        mesh.vertices.front()
            .position;

    core::Vec3 maximum =
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

bool decode_gltf_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
    std::string* error) {

    mesh = {};

    if (!asset.guid.valid()) {
        set_error(
            error,
            "glTF model AssetGuid is invalid");
        return false;
    }

    GltfSource source;

    if (!load_gltf_source(
            asset,
            source,
            error)) {
        return false;
    }

    JsonValue root;
    std::string json_error;

    JsonParser parser{
        source.json};

    if (!parser.parse(
            root,
            json_error)) {

        set_error(
            error,
            "glTF JSON parse failed: " +
                json_error);
        return false;
    }

    if (root.kind !=
        JsonValue::Kind::Object) {

        set_error(
            error,
            "glTF root must be a JSON object");
        return false;
    }

    const auto* asset_info =
        member(
            root,
            "asset");

    const auto version =
        asset_info
            ? string_value(
                member(
                    *asset_info,
                    "version"))
            : std::nullopt;

    if (!version ||
        version->rfind("2.", 0) !=
            0u) {

        set_error(
            error,
            "only glTF 2.x is supported");
        return false;
    }

    std::vector<
        std::vector<std::uint8_t>>
        buffers;

    if (!load_buffers(
            root,
            source,
            asset.source_path,
            buffers,
            error)) {
        return false;
    }

    std::vector<BufferView>
        views;

    if (!parse_buffer_views(
            root,
            views,
            error)) {
        return false;
    }

    std::vector<Accessor>
        accessors;

    if (!parse_accessors(
            root,
            accessors,
            error)) {
        return false;
    }

    MeshData decoded;

    if (!append_meshes(
            root,
            accessors,
            views,
            buffers,
            decoded,
            error)) {
        return false;
    }

    calculate_bounds(
        decoded);

    if (!decoded.valid()) {
        set_error(
            error,
            "decoded glTF MeshData is invalid");
        return false;
    }

    mesh =
        std::move(decoded);

    return true;
}

bool decode_gltf_base_color_texture(
    const ResolvedModelAsset& asset,
    DecodedTextureData& texture,
    std::string* error) {

    texture = {};

    if (!asset.guid.valid()) {
        set_error(error, "glTF texture model AssetGuid is invalid");
        return false;
    }

    GltfSource source;
    if (!load_gltf_source(asset, source, error)) {
        return false;
    }

    JsonValue root;
    std::string json_error;
    JsonParser parser{source.json};

    if (!parser.parse(root, json_error) ||
        root.kind != JsonValue::Kind::Object) {
        set_error(error, "glTF texture JSON parse failed: " + json_error);
        return false;
    }

    const auto* asset_info = member(root, "asset");
    const auto version = asset_info
        ? string_value(member(*asset_info, "version"))
        : std::nullopt;

    if (!version || version->rfind("2.", 0) != 0u) {
        set_error(error, "glTF texture requires glTF 2.x");
        return false;
    }

    // Our first mesh path merges the primitives into one draw. For now
    // only the first primitive's base-color map is used for that draw.
    const auto* meshes = member(root, "meshes");

    if (!meshes || meshes->kind != JsonValue::Kind::Array ||
        meshes->array.empty()) {
        set_error(error, "glTF texture has no mesh");
        return false;
    }

    const auto* primitives = member(meshes->array.front(), "primitives");

    if (!primitives ||
        primitives->kind != JsonValue::Kind::Array ||
        primitives->array.empty()) {
        set_error(error, "glTF texture has no mesh primitive");
        return false;
    }

    const auto material_index =
        index_value(member(primitives->array.front(), "material"));
    const auto* materials = member(root, "materials");

    if (!material_index || !materials ||
        materials->kind != JsonValue::Kind::Array ||
        *material_index >= materials->array.size()) {
        set_error(error, "glTF mesh has no base-color material");
        return false;
    }

    const auto* pbr = member(
        materials->array[*material_index],
        "pbrMetallicRoughness");

    // glTF baseColorFactor values are linear. Vulkan SRGB textures
    // convert to linear on sampling, so bake the tint in linear space
    // and encode it back to SRGB for the existing sampler path.
    std::array<double, 4> factor{1.0, 1.0, 1.0, 1.0};
    if (const auto* json_factor = pbr
            ? member(*pbr, "baseColorFactor")
            : nullptr) {
        if (json_factor->kind != JsonValue::Kind::Array ||
            json_factor->array.size() != 4u) {
            set_error(error, "glTF baseColorFactor must contain four values");
            return false;
        }
        for (std::size_t i = 0; i < 4u; ++i) {
            const auto& value = json_factor->array[i];
            if (value.kind != JsonValue::Kind::Number ||
                !std::isfinite(value.number) ||
                value.number < 0.0 || value.number > 1.0) {
                set_error(error, "glTF baseColorFactor component is invalid");
                return false;
            }
            factor[i] = value.number;
        }
    }

    const auto encode_srgb = [](double linear) {
        linear = std::clamp(linear, 0.0, 1.0);
        const double encoded = linear <= 0.0031308
            ? 12.92 * linear
            : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        return static_cast<std::uint8_t>(
            std::lround(std::clamp(encoded, 0.0, 1.0) * 255.0));
    };

    const auto decode_srgb = [](double encoded) {
        return encoded <= 0.04045
            ? encoded / 12.92
            : std::pow((encoded + 0.055) / 1.055, 2.4);
    };

    const auto* base_color = pbr
        ? member(*pbr, "baseColorTexture")
        : nullptr;

    const auto texture_index = base_color
        ? index_value(member(*base_color, "index"))
        : std::nullopt;

    if (!base_color) {
        // A color-only PBR material (no image) still has a visible
        // base color. Synthesize a 1x1 texture without shader changes.
        texture.width = 1u;
        texture.height = 1u;
        texture.color_space = DecodedTextureColorSpace::SRgb;
        texture.rgba8 = {
            encode_srgb(factor[0]),
            encode_srgb(factor[1]),
            encode_srgb(factor[2]),
            static_cast<std::uint8_t>(
                std::lround(factor[3] * 255.0))
        };
        return true;
    }

    const auto* textures = member(root, "textures");

    if (!texture_index || !textures ||
        textures->kind != JsonValue::Kind::Array ||
        *texture_index >= textures->array.size()) {
        set_error(error, "glTF mesh material has no base-color texture");
        return false;
    }

    const auto image_index = index_value(
        member(textures->array[*texture_index], "source"));
    const auto* images = member(root, "images");

    if (!image_index || !images ||
        images->kind != JsonValue::Kind::Array ||
        *image_index >= images->array.size()) {
        set_error(error, "glTF base-color image source is unavailable");
        return false;
    }

    const auto& image = images->array[*image_index];
    const auto mime = string_value(member(image, "mimeType"));

    if (mime && *mime != "image/png" && *mime != "image/jpeg") {
        set_error(error, "glTF base-color image must be PNG or JPEG");
        return false;
    }

    std::vector<std::uint8_t> image_bytes;
    const auto image_view = index_value(member(image, "bufferView"));
    const auto image_uri = string_value(member(image, "uri"));

    if (image_view) {
        if (!mime) {
            set_error(error, "glTF bufferView image requires mimeType");
            return false;
        }

        std::vector<BufferView> views;
        std::vector<std::vector<std::uint8_t>> buffers;

        if (!parse_buffer_views(root, views, error) ||
            !load_buffers(root, source, asset.source_path, buffers, error)) {
            return false;
        }

        if (*image_view >= views.size()) {
            set_error(error, "glTF image bufferView index is invalid");
            return false;
        }

        const auto& view = views[*image_view];

        if (view.buffer >= buffers.size() ||
            view.offset > buffers[view.buffer].size() ||
            view.length > buffers[view.buffer].size() - view.offset) {
            set_error(error, "glTF image bufferView is outside its buffer");
            return false;
        }

        const auto& bytes = buffers[view.buffer];
        image_bytes.assign(
            bytes.begin() + static_cast<std::ptrdiff_t>(view.offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(
                view.offset + view.length));
    } else if (image_uri) {
        if (image_uri->rfind("data:", 0) == 0u) {
            const auto comma = image_uri->find(',');

            if (comma == std::string::npos ||
                image_uri->substr(0, comma).find(";base64") ==
                    std::string::npos) {
                set_error(error, "glTF image data URI must be base64");
                return false;
            }

            if (!decode_base64(
                    std::string_view{*image_uri}.substr(comma + 1u),
                    image_bytes,
                    error)) {
                return false;
            }
        } else {
            const auto path = gltf_sidecar_path(
                asset.source_path,
                *image_uri,
                error);

            if (!path || !read_binary_file(
                    *path,
                    image_bytes,
                    error)) {
                return false;
            }
        }
    } else {
        set_error(error, "glTF image has neither bufferView nor URI");
        return false;
    }

    if (image_bytes.empty() || image_bytes.size() > INT_MAX) {
        set_error(error, "glTF base-color image payload is invalid");
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load_from_memory(
        image_bytes.data(),
        static_cast<int>(image_bytes.size()),
        &width,
        &height,
        &channels,
        4);

    if (!pixels || width <= 0 || height <= 0 ||
        static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height) >
            std::numeric_limits<std::size_t>::max() / 4u) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        set_error(error, "glTF PNG/JPEG base-color decode failed");
        return false;
    }

    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.color_space = DecodedTextureColorSpace::SRgb;
    texture.rgba8.assign(
        pixels,
        pixels + static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) * 4u);

    stbi_image_free(pixels);

    for (std::size_t pixel = 0u;
         pixel < texture.rgba8.size(); pixel += 4u) {
        for (std::size_t channel = 0u; channel < 3u; ++channel) {
            const double encoded =
                static_cast<double>(texture.rgba8[pixel + channel]) / 255.0;
            texture.rgba8[pixel + channel] = encode_srgb(
                decode_srgb(encoded) * factor[channel]);
        }
        texture.rgba8[pixel + 3u] =
            static_cast<std::uint8_t>(std::lround(
                static_cast<double>(texture.rgba8[pixel + 3u]) *
                factor[3]));
    }
    return true;
}

} // namespace nengine::render
