#version 400 core

in vec3 fragPos;
in vec3 fragNormal;
in vec3 fragTangent;
in float fragTangentSign;
in vec2 fragTexCoord;
in vec3 fragFaceForward;
in vec3 fragFaceRight;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 indirectDiffuse;
// Transparent surfaces attenuate the background contribution through the shared blend state.
// Their own indirect light is excluded because the AO depth describes the opaque surface.
uniform bool opaqueForSsao;

struct MaterialData
{
    vec3 baseColor;
    vec3 specularColor;
    float shininess;
    float environmentReflectivity;
    float metallic;
    float roughness;
    float ambientOcclusion;
    float normalScale;
    float opacity;
    int shadingModel;
    int surfaceSide; // Front=0, Back=1, Double=2, Legacy=3.
    float toonThreshold;
    float toonShadowStrength;
    vec3 toonShadowColor;
    float rimLightStrength;
    vec3 rimLightColor;
    bool faceShadowEnabled;
    float faceShadowSoftness;
    bool faceShadowMirrorX;

    sampler2D albedoMap;
    sampler2D ormMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D displacementMap;
    sampler2D faceShadowMap;
    bool hasAlbedoMap;
    bool hasOrmMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasDisplacementMap;
    bool hasFaceShadowMap;
};

uniform MaterialData material;

const int MAX_FORWARD_LIGHTS = 16;
const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;

struct LightData
{
    vec4 positionAndType;
    vec4 directionAndRange;
    vec4 colorAndIntensity;
    vec4 spotAnglesAndShadow;
};

layout(std140) uniform ForwardLights
{
    LightData lights[MAX_FORWARD_LIGHTS];
};

uniform int lightCount;
uniform int keyLightIndex;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLut;
uniform float prefilterMaxLod;
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
uniform bool wireframePass;


// Inverse yaw maps a world-space direction into the authored environment.
uniform vec3 environmentTransform; // cos(yaw), sin(yaw), linear intensity
vec3 EnvironmentDirection(vec3 d)
{
    return vec3(environmentTransform.x*d.x - environmentTransform.y*d.z,
        d.y, environmentTransform.y*d.x + environmentTransform.x*d.z);
}

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float nDotH = max(dot(N, H), 0.0);
    float nDotHSquared = nDotH * nDotH;
    float denominator = nDotHSquared * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.000001);
}

float GeometrySchlickGGX(float nDotDirection, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotDirection / max(nDotDirection * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float nDotV = max(dot(N, V), 0.0);
    float nDotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(nDotV, roughness) *
        GeometrySchlickGGX(nDotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
}

float CalculateDistanceAttenuation(float distanceToLight, float range)
{
    float normalizedDistance = distanceToLight / max(range, 0.0001);
    float window = clamp(
        1.0 - pow(normalizedDistance, 4.0), 0.0, 1.0);
    return window * window;
}

float CalculateSpotAttenuation(
    vec3 surfaceToLight,
    vec3 lightDirection,
    float innerConeCos,
    float outerConeCos)
{
    vec3 lightToSurface = -surfaceToLight;
    float coneCos = dot(lightToSurface, normalize(lightDirection));
    return smoothstep(outerConeCos, innerConeCos, coneCos);
}

vec3 ResolveAuthoredSurfaceNormal()
{
    vec3 N = normalize(fragNormal);
    if (!material.hasNormalMap)
        return N;

    vec3 T = normalize(fragTangent - N * dot(N, fragTangent));
    vec3 B = normalize(cross(N, T)) * fragTangentSign;
    vec3 tangentNormal =
        texture(material.normalMap, fragTexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= clamp(material.normalScale, 0.0, 2.0);
    tangentNormal = normalize(tangentNormal);
    return normalize(mat3(T, B, N) * tangentNormal);
}

vec3 ResolveSurfaceNormal()
{
    vec3 N = ResolveAuthoredSurfaceNormal();
    // Raster front winding already includes model/view reflection parity.
    // Flip the complete perturbed normal: this is equivalent to reversing
    // all three TBN axes, rather than only the geometric normal.
    if (material.surfaceSide == 1 ||
        (material.surfaceSide == 2 && !gl_FrontFacing))
        return -N;
    return N;
}

float CalculateFaceLightBand(vec3 lightDirection)
{
    vec3 faceForward = normalize(fragFaceForward);
    vec3 faceRight = normalize(
        fragFaceRight - faceForward * dot(fragFaceRight, faceForward));
    vec2 faceLight = vec2(
        dot(lightDirection, faceRight),
        dot(lightDirection, faceForward));
    float projectedLength = length(faceLight);
    if (projectedLength <= 0.0001)
        return -1.0;

    faceLight /= projectedLength;
    bool mirrorU = faceLight.x < 0.0;
    if (material.faceShadowMirrorX)
        mirrorU = !mirrorU;
    vec2 faceUv = fragTexCoord;
    if (mirrorU)
        faceUv.x = 1.0 - faceUv.x;

    float lightAngle = abs(atan(faceLight.x, faceLight.y)) / PI;
    float sdfThreshold = texture(material.faceShadowMap, faceUv).r;
    float softness = max(material.faceShadowSoftness, 0.0001);
    return smoothstep(
        lightAngle - softness,
        lightAngle + softness,
        sdfThreshold);
}


uniform bool coverageMasked;
uniform float coverageOpacity;
uniform float coverageCutoff;
uniform bool coverageHasMap;
uniform sampler2D coverageMap;

void main()
{
    if (coverageMasked && coverageOpacity * (coverageHasMap ? texture(coverageMap, fragTexCoord).a : 1.0) < coverageCutoff)
        discard;

    indirectDiffuse = vec4(0.0);
    if (wireframePass) {
        color = vec4(0.03, 0.03, 0.03, 1.0);
        return;
    }

    vec4 albedoSample = vec4(1.0);
    if (material.hasAlbedoMap)
        albedoSample = texture(material.albedoMap, fragTexCoord);
    vec3 albedo = pow(max(material.baseColor, vec3(0.0)), vec3(2.2)) *
        albedoSample.rgb;

    vec3 N = ResolveSurfaceNormal();
    vec3 V = normalize(-fragPos);
    vec3 worldPosition = (inverseView * vec4(fragPos, 1.0)).xyz;
    float linearDepth = max(-fragPos.z, 0.0);

    if (material.shadingModel == 1)
    {
        vec3 toonLighting = vec3(0.03) * albedo;
        for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            LightData light = lights[lightIndex];
            int lightType = int(light.positionAndType.w + 0.5);
            vec3 L;
            float attenuation = 1.0;
            if (lightType == LIGHT_TYPE_DIRECTIONAL)
                L = normalize(-light.directionAndRange.xyz);
            else
            {
                vec3 toLight = light.positionAndType.xyz - fragPos;
                float distanceToLight = length(toLight);
                L = distanceToLight > 0.0001 ? toLight / distanceToLight : vec3(0.0, 1.0, 0.0);
                attenuation = CalculateDistanceAttenuation(distanceToLight, light.directionAndRange.w);
                if (lightType == LIGHT_TYPE_SPOT)
                    attenuation *= CalculateSpotAttenuation(L, light.directionAndRange.xyz,
                        light.spotAnglesAndShadow.x, light.spotAnglesAndShadow.y);
            }
            float nDotL = max(dot(N, L), 0.0) * attenuation;
            float visibility = CalculateShadowVisibility(int(light.spotAnglesAndShadow.w),
                worldPosition, linearDepth, dot(N, L), false);
            float lightBand = step(clamp(material.toonThreshold, 0.0, 1.0), nDotL);
            if (lightIndex == keyLightIndex &&
                material.faceShadowEnabled &&
                material.hasFaceShadowMap)
            {
                float faceLightBand = CalculateFaceLightBand(L);
                if (faceLightBand >= 0.0)
                    lightBand = faceLightBand;
            }
            vec3 shadowBand = mix(material.toonShadowColor, vec3(1.0),
                1.0 - material.toonShadowStrength);
            vec3 bandColor = mix(shadowBand, vec3(1.0), lightBand);
            vec3 radiance = light.colorAndIntensity.rgb *
                light.colorAndIntensity.w * attenuation;
            toonLighting += albedo * bandColor * radiance * visibility;
        }
        float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * material.rimLightStrength;
        indirectDiffuse = vec4(opaqueForSsao ? vec3(0.03) * albedo : vec3(0.0),
            clamp(material.opacity * albedoSample.a, 0.0, 1.0));
        color = vec4(ApplyCascadeDebug(toonLighting + material.rimLightColor * rim, linearDepth),
            clamp(material.opacity * albedoSample.a, 0.0, 1.0));
        return;
    }

    vec3 specularTint = clamp(material.specularColor, vec3(0.0), vec3(1.0));
    if (material.hasSpecularMap)
        specularTint *= texture(material.specularMap, fragTexCoord).rgb;

    vec3 ormSample = vec3(1.0);
    if (material.hasOrmMap)
        ormSample = texture(material.ormMap, fragTexCoord).rgb;
    float metallic = clamp(material.metallic * ormSample.b, 0.0, 1.0);
    float roughness = clamp(
        material.roughness * ormSample.g, 0.045, 1.0);
    float ao = clamp(
        material.ambientOcclusion * ormSample.r, 0.0, 1.0);

    vec3 f0 = mix(vec3(0.04) * specularTint, albedo, metallic);
    float nDotV = max(dot(N, V), 0.0);
    vec3 directLighting = vec3(0.0);
    for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
    {
        LightData light = lights[lightIndex];
        int lightType = int(light.positionAndType.w + 0.5);
        vec3 L;
        float attenuation = 1.0;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-light.directionAndRange.xyz);
        }
        else
        {
            vec3 toLight = light.positionAndType.xyz - fragPos;
            float distanceToLight = length(toLight);
            L = distanceToLight > 0.0001
                ? toLight / distanceToLight
                : vec3(0.0, 1.0, 0.0);
            attenuation = CalculateDistanceAttenuation(
                distanceToLight, light.directionAndRange.w);
            if (lightType == LIGHT_TYPE_SPOT)
            {
                attenuation *= CalculateSpotAttenuation(
                    L,
                    light.directionAndRange.xyz,
                    light.spotAnglesAndShadow.x,
                    light.spotAnglesAndShadow.y);
            }
        }

        float nDotL = max(dot(N, L), 0.0);
        if (nDotL <= 0.0 || attenuation <= 0.0)
            continue;

        vec3 H = normalize(V + L);
        float distribution = DistributionGGX(N, H, roughness);
        float geometry = GeometrySmith(N, V, L, roughness);
        vec3 fresnel = FresnelSchlick(max(dot(H, V), 0.0), f0);
        vec3 specular = distribution * geometry * fresnel /
            max(4.0 * nDotV * nDotL, 0.0001);
        vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
        float visibility = CalculateShadowVisibility(int(light.spotAnglesAndShadow.w),
            worldPosition, linearDepth, dot(N, L), false);
        vec3 radiance = light.colorAndIntensity.rgb *
            light.colorAndIntensity.w * attenuation;
        directLighting += (diffuseWeight * albedo / PI + specular) *
            radiance * nDotL * visibility;
    }

    vec3 reflectDirView = reflect(-V, N);
    vec3 reflectDirWorld = EnvironmentDirection(normalize(mat3(inverseView) * reflectDirView));
    vec3 normalWorld = EnvironmentDirection(normalize(mat3(inverseView) * N));
    vec3 environmentFresnel = f0 + (max(vec3(1.0-roughness),f0)-f0) * pow(1.0-nDotV,5.0);
    vec3 diffuseWeight = (vec3(1.0)-environmentFresnel)*(1.0-metallic);
    // Irradiance map stores E/pi. All IBL textures contain linear scene radiance.
    vec3 environmentDiffuse = texture(irradianceMap,normalWorld).rgb * albedo * diffuseWeight * environmentTransform.z;
    vec3 envReflection = textureLod(prefilterMap,reflectDirWorld,roughness*prefilterMaxLod).rgb;
    vec2 integratedBrdf = texture(brdfLut,vec2(nDotV,roughness)).rg;
    vec3 environmentSpecular = envReflection * (f0*integratedBrdf.x+integratedBrdf.y) *
        material.environmentReflectivity * environmentTransform.z;
    vec3 ambient = (environmentDiffuse + environmentSpecular) * ao;

    indirectDiffuse = vec4(opaqueForSsao ? min(environmentDiffuse * ao, vec3(65504.0)) : vec3(0.0),
        clamp(material.opacity * albedoSample.a, 0.0, 1.0));
    color = vec4(
        ApplyCascadeDebug(min(ambient + directLighting, vec3(65504.0)), linearDepth),
        clamp(material.opacity * albedoSample.a, 0.0, 1.0));
}
