#version 450

layout(set = 0, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 0, binding = 1) uniform sampler2D normalTexture;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 3) uniform sampler2D emissiveTexture;
layout(set = 0, binding = 4) uniform sampler2D occlusionTexture;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec3 inViewPosition;
layout(location = 2) in vec3 inViewNormal;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;

float distributionGGX(
    vec3 normal,
    vec3 halfVector,
    float roughness) {

    float a =
        roughness * roughness;
    float a2 =
        a * a;

    float nDotH =
        max(
            dot(
                normal,
                halfVector),
            0.0);

    float nDotH2 =
        nDotH * nDotH;

    float denominator =
        nDotH2 *
            (a2 - 1.0) +
        1.0;

    return a2 /
        max(
            PI *
                denominator *
                denominator,
            1.0e-5);
}

float geometrySchlickGGX(
    float nDotV,
    float roughness) {

    float r =
        roughness + 1.0;

    float k =
        (r * r) /
        8.0;

    return nDotV /
        max(
            nDotV *
                (1.0 - k) +
            k,
            1.0e-5);
}

float geometrySmith(
    vec3 normal,
    vec3 viewDirection,
    vec3 lightDirection,
    float roughness) {

    float nDotV =
        max(
            dot(
                normal,
                viewDirection),
            0.0);

    float nDotL =
        max(
            dot(
                normal,
                lightDirection),
            0.0);

    return
        geometrySchlickGGX(
            nDotV,
            roughness) *
        geometrySchlickGGX(
            nDotL,
            roughness);
}

vec3 fresnelSchlick(
    float cosTheta,
    vec3 f0) {

    return f0 +
        (1.0 - f0) *
        pow(
            clamp(
                1.0 - cosTheta,
                0.0,
                1.0),
            5.0);
}

mat3 cotangentFrame(
    vec3 normal,
    vec3 position,
    vec2 uv) {

    vec3 dp1 =
        dFdx(position);

    vec3 dp2 =
        dFdy(position);

    vec2 duv1 =
        dFdx(uv);

    vec2 duv2 =
        dFdy(uv);

    vec3 dp2Perp =
        cross(
            dp2,
            normal);

    vec3 dp1Perp =
        cross(
            normal,
            dp1);

    vec3 tangent =
        dp2Perp *
            duv1.x +
        dp1Perp *
            duv2.x;

    vec3 bitangent =
        dp2Perp *
            duv1.y +
        dp1Perp *
            duv2.y;

    float maximum =
        max(
            dot(
                tangent,
                tangent),
            dot(
                bitangent,
                bitangent));

    if (maximum <= 1.0e-12) {
        vec3 fallback =
            abs(normal.z) < 0.999
                ? vec3(0.0, 0.0, 1.0)
                : vec3(0.0, 1.0, 0.0);

        tangent =
            normalize(
                cross(
                    fallback,
                    normal));

        bitangent =
            normalize(
                cross(
                    normal,
                    tangent));

        return mat3(
            tangent,
            bitangent,
            normal);
    }

    float inverseMaximum =
        inversesqrt(
            maximum);

    return mat3(
        tangent *
            inverseMaximum,
        bitangent *
            inverseMaximum,
        normal);
}

vec3 materialNormal() {
    vec3 normal =
        normalize(
            inViewNormal);

    if (!gl_FrontFacing) {
        normal =
            -normal;
    }

    vec3 tangentNormal =
        texture(
            normalTexture,
            inUv)
            .xyz *
        2.0 -
        1.0;

    if (dot(
            tangentNormal,
            tangentNormal) <=
        1.0e-8) {

        return normal;
    }

    mat3 tbn =
        cotangentFrame(
            normal,
            inViewPosition,
            inUv);

    return normalize(
        tbn *
        tangentNormal);
}

void main() {
    vec4 baseColor =
        texture(
            baseColorTexture,
            inUv);

    vec3 normal =
        materialNormal();

    vec4 metallicRoughness =
        texture(
            metallicRoughnessTexture,
            inUv);

    float roughness =
        clamp(
            metallicRoughness.g,
            0.045,
            1.0);

    float metallic =
        clamp(
            metallicRoughness.b,
            0.0,
            1.0);

    float occlusion =
        clamp(
            texture(
                occlusionTexture,
                inUv)
                .r,
            0.0,
            1.0);

    vec3 emissive =
        texture(
            emissiveTexture,
            inUv)
            .rgb;

    vec3 viewDirection =
        normalize(
            -inViewPosition);

    vec3 lightDirection =
        normalize(
            vec3(
                -0.45,
                0.70,
                -0.55));

    vec3 halfVector =
        normalize(
            viewDirection +
            lightDirection);

    vec3 f0 =
        mix(
            vec3(0.04),
            baseColor.rgb,
            metallic);

    vec3 fresnel =
        fresnelSchlick(
            max(
                dot(
                    halfVector,
                    viewDirection),
                0.0),
            f0);

    float distribution =
        distributionGGX(
            normal,
            halfVector,
            roughness);

    float geometry =
        geometrySmith(
            normal,
            viewDirection,
            lightDirection,
            roughness);

    float nDotV =
        max(
            dot(
                normal,
                viewDirection),
            0.0);

    float nDotL =
        max(
            dot(
                normal,
                lightDirection),
            0.0);

    vec3 specular =
        distribution *
        geometry *
        fresnel /
        max(
            4.0 *
                nDotV *
                nDotL,
            1.0e-4);

    vec3 diffuseWeight =
        (vec3(1.0) -
         fresnel) *
        (1.0 - metallic);

    vec3 direct =
        (diffuseWeight *
             baseColor.rgb /
             PI +
         specular) *
        nDotL *
        vec3(4.0);

    vec3 ambient =
        0.03 *
        baseColor.rgb *
        occlusion;

    vec3 color =
        ambient +
        direct +
        emissive;

    color =
        color /
        (color +
         vec3(1.0));

    outColor =
        vec4(
            color,
            baseColor.a);
}
