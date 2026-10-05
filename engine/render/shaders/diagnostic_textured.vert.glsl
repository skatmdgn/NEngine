#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec2 outUv;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    outUv = inUv;
    gl_Position =
        pc.mvp *
        vec4(inPosition, 1.0);
}
