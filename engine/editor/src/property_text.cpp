#include "nengine/editor/property_text.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace nengine::editor {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::string normalized_vector_text(
    std::string_view text) {

    std::string result{text};
    for (auto& ch : result) {
        if (ch == ',' || ch == ';') {
            ch = ' ';
        }
    }
    return result;
}

template <typename T>
bool parse_integral(
    std::string_view text,
    T& value) {

    const auto begin = text.data();
    const auto end = begin + text.size();

    const auto parsed =
        std::from_chars(
            begin,
            end,
            value);

    return parsed.ec == std::errc{} &&
        parsed.ptr == end;
}

bool parse_double(
    std::string_view text,
    double& value) {

    std::string owned{text};
    if (owned.empty()) return false;

    char* end = nullptr;
    value = std::strtod(
        owned.c_str(),
        &end);

    return end &&
        end != owned.c_str() &&
        *end == '\0' &&
        std::isfinite(value);
}

} // namespace

std::string format_property_value(
    const core::PropertyValue& value) {

    if (std::holds_alternative<
            std::monostate>(value)) {
        return "<null>";
    }

    if (const auto* typed =
            std::get_if<bool>(&value)) {
        return *typed ? "true" : "false";
    }

    if (const auto* typed =
            std::get_if<std::int64_t>(&value)) {
        return std::to_string(*typed);
    }

    if (const auto* typed =
            std::get_if<std::uint64_t>(&value)) {
        return std::to_string(*typed);
    }

    if (const auto* typed =
            std::get_if<double>(&value)) {

        std::ostringstream stream;
        stream
            << std::setprecision(17)
            << *typed;
        return stream.str();
    }

    if (const auto* typed =
            std::get_if<std::string>(&value)) {
        return *typed;
    }

    if (const auto* typed =
            std::get_if<core::Vec3>(&value)) {

        std::ostringstream stream;
        stream
            << std::setprecision(9)
            << typed->x << ' '
            << typed->y << ' '
            << typed->z;
        return stream.str();
    }

    if (const auto* typed =
            std::get_if<core::Quat>(&value)) {

        std::ostringstream stream;
        stream
            << std::setprecision(9)
            << typed->x << ' '
            << typed->y << ' '
            << typed->z << ' '
            << typed->w;
        return stream.str();
    }

    if (const auto* typed =
            std::get_if<core::Entity>(&value)) {

        if (!typed->valid()) {
            return "none";
        }

        return
            "entity:" +
            std::to_string(
                typed->value);
    }

    return "<unsupported>";
}

bool parse_property_value(
    core::PropertyKind kind,
    std::string_view text,
    core::PropertyValue& value,
    std::string* error) {

    switch (kind) {
    case core::PropertyKind::Boolean:
        if (text == "true" ||
            text == "1" ||
            text == "yes" ||
            text == "on") {
            value = true;
            return true;
        }

        if (text == "false" ||
            text == "0" ||
            text == "no" ||
            text == "off") {
            value = false;
            return true;
        }

        set_error(
            error,
            "expected boolean true/false");
        return false;

    case core::PropertyKind::Integer: {
        std::int64_t parsed = 0;

        if (!parse_integral(
                text,
                parsed)) {
            set_error(
                error,
                "expected signed integer");
            return false;
        }

        value = parsed;
        return true;
    }

    case core::PropertyKind::UnsignedInteger: {
        std::uint64_t parsed = 0;

        if (!parse_integral(
                text,
                parsed)) {
            set_error(
                error,
                "expected unsigned integer");
            return false;
        }

        value = parsed;
        return true;
    }

    case core::PropertyKind::Float: {
        double parsed = 0.0;

        if (!parse_double(
                text,
                parsed)) {
            set_error(
                error,
                "expected finite floating-point number");
            return false;
        }

        value = parsed;
        return true;
    }

    case core::PropertyKind::String:
    case core::PropertyKind::AssetReference:
        value = std::string{text};
        return true;

    case core::PropertyKind::Vec3: {
        const auto normalized =
            normalized_vector_text(text);

        std::istringstream input{
            normalized};

        core::Vec3 parsed{};

        if (!(input
                >> parsed.x
                >> parsed.y
                >> parsed.z)) {
            set_error(
                error,
                "expected three numbers: x y z");
            return false;
        }

        std::string extra;
        if (input >> extra) {
            set_error(
                error,
                "too many Vec3 values");
            return false;
        }

        value = parsed;
        return true;
    }

    case core::PropertyKind::Quaternion: {
        const auto normalized =
            normalized_vector_text(text);

        std::istringstream input{
            normalized};

        core::Quat parsed{};

        if (!(input
                >> parsed.x
                >> parsed.y
                >> parsed.z
                >> parsed.w)) {
            set_error(
                error,
                "expected four numbers: x y z w");
            return false;
        }

        std::string extra;
        if (input >> extra) {
            set_error(
                error,
                "too many Quaternion values");
            return false;
        }

        value = parsed;
        return true;
    }

    case core::PropertyKind::EntityReference:
        if (text == "none" ||
            text == "null" ||
            text.empty()) {

            value =
                core::Entity::invalid();

            return true;
        }

        if (text.starts_with(
                "entity:")) {

            std::uint64_t raw = 0;

            if (!parse_integral(
                    text.substr(7),
                    raw)) {
                set_error(
                    error,
                    "invalid entity handle");
                return false;
            }

            value =
                core::Entity{raw};

            return true;
        }

        set_error(
            error,
            "expected none or entity:<handle>");
        return false;
    }

    set_error(
        error,
        "unsupported property kind");
    return false;
}

} // namespace nengine::editor
