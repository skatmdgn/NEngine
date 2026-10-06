#include "nengine/assets/gltf_sidecars.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nengine::assets {
namespace {

void set_error(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

// This small JSON walker reads only the top-level buffers/images resource
// references. Other values are validated structurally and skipped. It
// deliberately does not treat arbitrary "uri" fields (extras/extensions)
// as model dependencies.
class ResourceJsonReader {
public:
    explicit ResourceJsonReader(std::string_view text) : text_(text) {}

    bool scan(std::vector<std::string>& uris) {
        whitespace();
        if (!consume('{')) return false;
        whitespace();
        if (consume('}')) return position_ == text_.size();

        for (;;) {
            std::string key;
            if (!string(key)) return false;
            whitespace();
            if (!consume(':')) return false;

            if (key == "buffers" || key == "images") {
                if (!resource_array(uris)) return false;
            } else if (!skip_value(0)) {
                return false;
            }

            whitespace();
            if (consume('}')) {
                whitespace();
                return position_ == text_.size();
            }
            if (!consume(',')) return false;
            whitespace();
        }
    }

private:
    void whitespace() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' ||
                text_[position_] == '\n' ||
                text_[position_] == '\r' ||
                text_[position_] == '\t')) {
            ++position_;
        }
    }

    bool consume(char ch) {
        if (position_ >= text_.size() || text_[position_] != ch) return false;
        ++position_;
        return true;
    }

    bool append_codepoint(std::uint32_t cp, std::string& output) {
        if (cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu)) return false;
        if (cp < 0x80u) {
            output.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            output.push_back(static_cast<char>(0xc0u | (cp >> 6u)));
            output.push_back(static_cast<char>(0x80u | (cp & 63u)));
        } else if (cp < 0x10000u) {
            output.push_back(static_cast<char>(0xe0u | (cp >> 12u)));
            output.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 63u)));
            output.push_back(static_cast<char>(0x80u | (cp & 63u)));
        } else {
            output.push_back(static_cast<char>(0xf0u | (cp >> 18u)));
            output.push_back(static_cast<char>(0x80u | ((cp >> 12u) & 63u)));
            output.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 63u)));
            output.push_back(static_cast<char>(0x80u | (cp & 63u)));
        }
        return true;
    }

    bool four_hex(std::uint32_t& cp) {
        if (text_.size() - position_ < 4u) return false;
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = text_[position_++];
            int digit = -1;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
            if (digit < 0) return false;
            cp = (cp << 4u) | static_cast<std::uint32_t>(digit);
        }
        return true;
    }

    bool string(std::string& output) {
        if (!consume('"')) return false;
        output.clear();
        for (; position_ < text_.size();) {
            const unsigned char ch =
                static_cast<unsigned char>(text_[position_++]);
            if (ch == '"') return true;
            if (ch < 0x20u) return false;
            if (ch != '\\') {
                output.push_back(static_cast<char>(ch));
                continue;
            }
            if (position_ == text_.size()) return false;
            const char escaped = text_[position_++];
            switch (escaped) {
            case '"': case '\\': case '/': output.push_back(escaped); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                std::uint32_t cp = 0;
                if (!four_hex(cp)) return false;
                if (cp >= 0xd800u && cp <= 0xdbffu) {
                    if (!consume('\\') || !consume('u')) return false;
                    std::uint32_t low = 0;
                    if (!four_hex(low) || low < 0xdc00u || low > 0xdfffu)
                        return false;
                    cp = 0x10000u +
                        ((cp - 0xd800u) << 10u) +
                        (low - 0xdc00u);
                }
                if (!append_codepoint(cp, output)) return false;
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool skip_value(int depth) {
        if (depth > 64) return false;
        whitespace();
        if (position_ >= text_.size()) return false;
        const char ch = text_[position_];

        if (ch == '"') {
            std::string ignored;
            return string(ignored);
        }
        if (ch == '{' || ch == '[') {
            const bool object = ch == '{';
            ++position_;
            whitespace();
            if (consume(object ? '}' : ']')) return true;
            for (;;) {
                if (object) {
                    std::string key;
                    if (!string(key)) return false;
                    whitespace();
                    if (!consume(':')) return false;
                }
                if (!skip_value(depth + 1)) return false;
                whitespace();
                if (consume(object ? '}' : ']')) return true;
                if (!consume(',')) return false;
                whitespace();
            }
        }
        for (const auto literal : {"true", "false", "null"}) {
            const std::string_view token{literal};
            if (text_.substr(position_, token.size()) == token) {
                position_ += token.size();
                return true;
            }
        }
        // Numbers are not used here; skip a lexical numeric token while
        // preserving object/array boundaries.
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            ++position_;
            while (position_ < text_.size()) {
                const char next = text_[position_];
                if ((next >= '0' && next <= '9') ||
                    next == '.' || next == 'e' || next == 'E' ||
                    next == '+' || next == '-') ++position_;
                else break;
            }
            return true;
        }
        return false;
    }

    bool resource_array(std::vector<std::string>& uris) {
        whitespace();
        if (!consume('[')) return false;
        whitespace();
        if (consume(']')) return true;

        for (;;) {
            whitespace();
            if (!consume('{')) return false;
            whitespace();

            if (!consume('}')) {
                for (;;) {
                    std::string field;
                    if (!string(field)) return false;
                    whitespace();
                    if (!consume(':')) return false;
                    whitespace();

                    if (field == "uri") {
                        std::string uri;
                        if (!string(uri)) return false;
                        if (uri.size() > 8192u) return false;
                        uris.push_back(std::move(uri));
                        if (uris.size() > 2048u) return false;
                    } else if (!skip_value(0)) {
                        return false;
                    }

                    whitespace();
                    if (consume('}')) break;
                    if (!consume(',')) return false;
                    whitespace();
                }
            }

            whitespace();
            if (consume(']')) return true;
            if (!consume(',')) return false;
            whitespace();
        }
    }

    std::string_view text_;
    std::size_t position_{0};
};

bool valid_relative_uri(
    std::string_view uri,
    std::filesystem::path& relative) {

    if (uri.empty() || uri.size() > 4096u ||
        uri.find(':') != std::string_view::npos ||
        uri.find('\\') != std::string_view::npos ||
        uri.find('%') != std::string_view::npos ||
        uri.find('?') != std::string_view::npos ||
        uri.find('#') != std::string_view::npos) {
        return false;
    }

    relative = std::filesystem::u8path(uri.begin(), uri.end());
    if (relative.empty() || relative.is_absolute() ||
        relative.has_root_path()) return false;

    for (const auto& component : relative) {
        if (component == "." || component == ".." || component.empty())
            return false;
    }
    return true;
}

bool is_below(
    const std::filesystem::path& child,
    const std::filesystem::path& parent) {

    const auto relative = child.lexically_relative(parent);
    if (relative.empty() || relative == ".") return false;
    for (const auto& part : relative) {
        if (part == "..") return false;
    }
    return !relative.has_root_path();
}

} // namespace

bool collect_gltf_sidecars(
    const std::filesystem::path& source_gltf,
    std::vector<GltfSidecar>& sidecars,
    std::string* error) {

    sidecars.clear();
    std::error_code ec;
    const auto size = std::filesystem::file_size(source_gltf, ec);
    if (ec || size > 32u * 1024u * 1024u) {
        set_error(error, "glTF JSON source is unavailable or exceeds 32 MiB");
        return false;
    }

    std::ifstream input(source_gltf, std::ios::binary);
    if (!input) {
        set_error(error, "could not open glTF source for dependencies");
        return false;
    }

    std::string json(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});

    std::vector<std::string> uris;
    ResourceJsonReader reader{json};
    if (!reader.scan(uris)) {
        set_error(error, "malformed glTF JSON buffers/images resource references");
        return false;
    }

    const auto parent = std::filesystem::canonical(
        source_gltf.parent_path(), ec);
    if (ec) {
        set_error(error, "glTF source directory is unavailable");
        return false;
    }

    std::unordered_set<std::string> seen;

    for (const auto& uri : uris) {
        if (uri.rfind("data:", 0) == 0u) continue;

        std::filesystem::path relative;
        if (!valid_relative_uri(uri, relative)) {
            set_error(error, "unsafe or unsupported glTF external URI");
            return false;
        }

        // A symbolic link must not allow escaping the model's directory.
        const auto sidecar = std::filesystem::canonical(
            source_gltf.parent_path() / relative, ec);

        if (ec || !is_below(sidecar, parent) ||
            !std::filesystem::is_regular_file(sidecar, ec) || ec) {
            set_error(error,
                "glTF sidecar missing, non-file or escapes the model directory");
            return false;
        }

        const auto key = relative.generic_string();
        if (seen.insert(key).second) {
            sidecars.push_back({relative, sidecar});
        }
    }

    std::sort(sidecars.begin(), sidecars.end(),
        [](const GltfSidecar& a, const GltfSidecar& b) {
            return a.relative_path.generic_string() <
                b.relative_path.generic_string();
        });
    return true;
}

std::string gltf_sidecar_fingerprint(
    const std::filesystem::path& source_gltf) {

    std::vector<GltfSidecar> sidecars;
    std::string error;
    if (!collect_gltf_sidecars(source_gltf, sidecars, &error))
        return ":sidecar-error:" + error;

    std::ostringstream fingerprint;
    for (const auto& sidecar : sidecars) {
        std::error_code ec;
        const auto size =
            std::filesystem::file_size(sidecar.source_path, ec);
        if (ec) return ":sidecar-stat-error";
        const auto stamp =
            std::filesystem::last_write_time(sidecar.source_path, ec);
        if (ec) return ":sidecar-time-error";

        fingerprint << ":sidecar:" <<
            sidecar.relative_path.generic_string() << ':' <<
            size << ':' << stamp.time_since_epoch().count();
    }
    return fingerprint.str();
}

} // namespace nengine::assets
