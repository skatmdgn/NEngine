#pragma once

namespace nengine::core {

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    friend constexpr bool operator==(const Vec3&, const Vec3&) = default;
};

struct Quat {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};

    friend constexpr bool operator==(const Quat&, const Quat&) = default;
};

} // namespace nengine::core
