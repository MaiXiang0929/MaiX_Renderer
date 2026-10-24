// 反射地面片段着色器
// 使用 render-to-texture 的反射纹理 + cubemap 实现地面反射

#version 400 core

in vec3 worldPos;

uniform samplerCube cubemap;
uniform sampler2D reflectionTex;
uniform mat4 reflectionVP;
uniform vec3 cameraWorldPos;
uniform mat4 view;
uniform int groundShadowIndex;
uniform vec3 groundLightDirection;
// Forward shadow ABI: keep this block identical in pbr.frag and ground.frag.
struct ShadowData
{
    mat4 matrices[4];
    vec4 layerAndType; // base layer, light type, cascade count, cube index
    vec4 positionAndNear;
    vec4 farBiasNormalLegacy; // far, bias in texels, normal bias in texels, legacy
    vec4 splits;
    vec4 worldTexels;
    vec4 depthRanges;
    vec4 cascadeParameters; // near split, legacy perspective coefficient/cone width
};
layout(std140) uniform ForwardShadows
{
    ShadowData shadowLights[4];
};
uniform sampler2DArrayShadow shadowMap;
uniform samplerCubeArrayShadow shadowCubeMap;
uniform bool shadowsEnabled;
uniform int shadowLightCount;
uniform bool shadowDebugCascades;
uniform mat4 inverseView;

int ShadowCascadeIndex(ShadowData shadow, float linearDepth)
{
    int count = clamp(int(shadow.layerAndType.z + 0.5), 1, 4);
    for (int cascade = 0; cascade < count; ++cascade)
        if (linearDepth <= shadow.splits[cascade])
            return cascade;
    return -1;
}
float ShadowSliceStart(ShadowData shadow, int cascade)
{
    return cascade == 0 ? shadow.cascadeParameters.x : shadow.splits[cascade - 1];
}
float ShadowCascadeBlend(ShadowData shadow, int cascade, float linearDepth)
{
    float end = shadow.splits[cascade];
    float width = max((end - ShadowSliceStart(shadow, cascade)) * 0.1, 0.0001);
    return smoothstep(end - width, end, linearDepth);
}
float SampleProjectedShadow(ShadowData shadow, int cascade, vec3 worldPosition,
    float nDotL, bool ground)
{
    vec4 clip = shadow.matrices[cascade] * vec4(worldPosition, 1.0);
    if (clip.w <= 0.0 || shadow.layerAndType.x < 0.0)
        return 1.0;
    vec3 projected = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(projected, vec3(0.0))) || any(greaterThan(projected, vec3(1.0))))
        return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    bool spot = int(shadow.layerAndType.y + 0.5) == 2;
    bool legacy = shadow.farBiasNormalLegacy.w > 0.5;
    float slope = 1.0 - clamp(nDotL, -1.0, 1.0);
    float depthBias;
    if (legacy && !spot)
        depthBias = ground ? 0.001 : max(0.0025 * slope, 0.0005);
    else
    {
        float worldTexel = shadow.worldTexels[cascade];
        if (spot)
            worldTexel *= clip.w / max(shadow.farBiasNormalLegacy.x, 0.0001);
        if (legacy && spot)
            worldTexel = shadow.cascadeParameters.z * clip.w * texel.x;
        float worldBias = worldTexel * (legacy ? max(2.0 * slope, 0.5) :
            shadow.farBiasNormalLegacy.y + shadow.farBiasNormalLegacy.z * slope);
        if (spot)
        {
            worldBias = clamp(worldBias, 0.0, clip.w * 0.1);
            float nearPlane = shadow.positionAndNear.w;
            float farPlane = shadow.farBiasNormalLegacy.x;
            float coefficient = legacy ? shadow.cascadeParameters.y :
                nearPlane / (1.0 - nearPlane / farPlane);
            // clip.w is positive and worldBias is at most 10% of it. Work with
            // their ratio so a world-unit denominator floor cannot erase bias.
            float biasRatio = worldBias / clip.w;
            depthBias = (coefficient / clip.w) * biasRatio / (1.0 - biasRatio);
        }
        else
            depthBias = worldBias / max(shadow.depthRanges[cascade], 0.0001);
    }
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            visibility += texture(shadowMap, vec4(projected.xy + vec2(x, y) * texel,
                shadow.layerAndType.x + float(cascade), projected.z - depthBias));
    return visibility / 9.0;
}
float SamplePointShadow(ShadowData shadow, vec3 worldPosition, float nDotL)
{
    if (shadow.layerAndType.w < 0.0)
        return 1.0;
    vec3 fromLight = worldPosition - shadow.positionAndNear.xyz;
    float distance = length(fromLight);
    float nearPlane = shadow.positionAndNear.w;
    float farPlane = shadow.farBiasNormalLegacy.x;
    if (distance <= nearPlane || distance >= farPlane)
        return 1.0;
    vec3 direction = fromLight / distance;
    vec3 up = abs(direction.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(direction, up));
    vec3 bitangent = cross(direction, tangent);
    float angularTexel = 2.0 / float(textureSize(shadowCubeMap, 0).x);
    float worldTexel = angularTexel * distance;
    float worldBias = worldTexel * (shadow.farBiasNormalLegacy.y +
        shadow.farBiasNormalLegacy.z * (1.0 - clamp(nDotL, -1.0, 1.0)));
    float reference = clamp((distance - worldBias - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
    float visibility = 0.0;
    // Offsets are directions, so the cubemap selects neighboring faces across seams.
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            vec3 sampleDirection = direction + angularTexel * (float(x) * tangent + float(y) * bitangent);
            visibility += texture(shadowCubeMap, vec4(sampleDirection, shadow.layerAndType.w), reference);
        }
    return visibility / 9.0;
}
float CalculateShadowVisibility(int index, vec3 worldPosition, float linearDepth,
    float nDotL, bool ground)
{
    if (!shadowsEnabled || index < 0 || index >= shadowLightCount)
        return 1.0;
    ShadowData shadow = shadowLights[index];
    int type = int(shadow.layerAndType.y + 0.5);
    if (type == 1)
        return SamplePointShadow(shadow, worldPosition, nDotL);
    if (type == 2 || shadow.farBiasNormalLegacy.w > 0.5)
        return SampleProjectedShadow(shadow, 0, worldPosition, nDotL, ground);
    int cascade = ShadowCascadeIndex(shadow, linearDepth);
    if (cascade < 0)
        return 1.0;
    float visibility = SampleProjectedShadow(shadow, cascade, worldPosition, nDotL, ground);
    float blend = ShadowCascadeBlend(shadow, cascade, linearDepth);
    if (blend > 0.0)
    {
        int count = int(shadow.layerAndType.z + 0.5);
        float nextVisibility = cascade + 1 < count ?
            SampleProjectedShadow(shadow, cascade + 1, worldPosition, nDotL, ground) : 1.0;
        visibility = mix(visibility, nextVisibility, blend);
    }
    return visibility;
}
vec3 CascadeDebugColor(int cascade)
{
    if (cascade == 0) return vec3(0.9, 0.12, 0.08);
    if (cascade == 1) return vec3(0.12, 0.8, 0.14);
    if (cascade == 2) return vec3(0.1, 0.3, 0.95);
    return vec3(0.9, 0.7, 0.08);
}
vec3 ApplyCascadeDebug(vec3 result, float linearDepth)
{
    if (!shadowDebugCascades || !shadowsEnabled)
        return result;
    for (int index = 0; index < shadowLightCount; ++index)
    {
        ShadowData shadow = shadowLights[index];
        if (int(shadow.layerAndType.y + 0.5) != 0 || shadow.farBiasNormalLegacy.w > 0.5)
            continue;
        int cascade = ShadowCascadeIndex(shadow, linearDepth);
        if (cascade < 0)
            return result;
        vec3 tint = CascadeDebugColor(cascade);
        float blend = ShadowCascadeBlend(shadow, cascade, linearDepth);
        vec3 nextTint = cascade + 1 < int(shadow.layerAndType.z + 0.5) ?
            CascadeDebugColor(cascade + 1) : result;
        return mix(tint, nextTint, blend);
    }
    return result;
}
// End shared shadow ABI and sampling.

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 indirectDiffuse;


// Inverse yaw maps a world-space direction into the authored environment.
uniform vec3 environmentTransform; // cos(yaw), sin(yaw), linear intensity
vec3 EnvironmentDirection(vec3 d)
{
    return vec3(environmentTransform.x*d.x - environmentTransform.y*d.z,
        d.y, environmentTransform.y*d.x + environmentTransform.x*d.z);
}

void main()
{
    indirectDiffuse = vec4(0.0);
    // 将世界空间位置投影到反射纹理坐标
    vec4 clipPos = reflectionVP * vec4(worldPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    vec3 result;

    if (ndc.z > 0.0 && ndc.z < 1.0 &&
        uv.x >= 0.0 && uv.x <= 1.0 &&
        uv.y >= 0.0 && uv.y <= 1.0)
    {
        // 反射纹理中已有天空盒 + 物体的反射画面
        result = texture(reflectionTex, uv).rgb;
    }
    else
    {
        // 超出反射纹理范围时，回退到 cubemap 采样
        vec3 N = vec3(0.0, 1.0, 0.0);
        vec3 V = normalize(cameraWorldPos - worldPos);
        vec3 R = reflect(-V, N);
        result = texture(cubemap, EnvironmentDirection(R)).rgb * environmentTransform.z;
    }

    // 地面不写入 shadow map，只在此处接收 场景模型投下的阴影。
    float linearDepth = max(-(view * vec4(worldPos, 1.0)).z, 0.0);
    float nDotL = 1.0;
    if (groundShadowIndex >= 0 && groundShadowIndex < shadowLightCount)
    {
        ShadowData shadow = shadowLights[groundShadowIndex];
        vec3 L = int(shadow.layerAndType.y + 0.5) == 0 ? -groundLightDirection :
            shadow.positionAndNear.xyz - worldPos;
        nDotL = L.y / max(length(L), 0.0001);
    }
    float visibility = CalculateShadowVisibility(groundShadowIndex, worldPos, linearDepth, nDotL, true);
    result *= mix(0.35, 1.0, visibility);

    color = vec4(ApplyCascadeDebug(min(result, vec3(65504.0)), linearDepth), 1.0);
}
