#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out vec3 outViewNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 modelView;
} pc;

void main() {
    vec4 viewPosition =
        pc.modelView *
        vec4(inPosition, 1.0);

    outUv = inUv;
    outViewPosition =
        viewPosition.xyz;

    mat3 normalMatrix =
        transpose(
            inverse(
                mat3(pc.modelView)));

    outViewNormal =
        normalize(
            normalMatrix *
            inNormal);

    gl_Position =
        pc.mvp *
        vec4(inPosition, 1.0);
}
