#version 400 core

in vec3 fragPos;
in vec3 fragNormal;
in vec3 fragTangent;
in float fragTangentSign;
in vec2 fragTexCoord;
in vec4 fragLightSpacePos;

layout(location = 0) out vec4 color;

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

    sampler2D albedoMap;
    sampler2D ormMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D displacementMap;
    bool hasAlbedoMap;
    bool hasOrmMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasDisplacementMap;
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
uniform int shadowLightIndex;
uniform samplerCube cubemap;
uniform mat3 viewToWorld;
uniform sampler2DShadow shadowMap;
uniform bool shadowsEnabled;
uniform bool wireframePass;

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

float CalculateShadowVisibility(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    if (!shadowsEnabled)
        return 1.0;
    if (lightSpacePos.w <= 0.0)
        return 0.0;

    vec3 projected = lightSpacePos.xyz / lightSpacePos.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0 ||
        projected.z < 0.0 || projected.z > 1.0)
        return 0.0;

    float bias = max(0.0025 * (1.0 - dot(normal, lightDir)), 0.0005);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            visibility += texture(
                shadowMap,
                vec3(projected.xy + vec2(x, y) * texelSize, projected.z - bias));
        }
    }
    return visibility / 9.0;
}

vec3 ResolveSurfaceNormal()
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

void main()
{
    if (wireframePass) {
        color = vec4(0.03, 0.03, 0.03, 1.0);
        return;
    }

    vec4 albedoSample = vec4(1.0);
    if (material.hasAlbedoMap)
        albedoSample = texture(material.albedoMap, fragTexCoord);
    vec3 albedo = pow(max(material.baseColor, vec3(0.0)), vec3(2.2)) *
        albedoSample.rgb;

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

    vec3 N = ResolveSurfaceNormal();
    vec3 V = normalize(-fragPos);

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
        float visibility = lightIndex == shadowLightIndex
            ? CalculateShadowVisibility(fragLightSpacePos, N, L)
            : 1.0;
        vec3 radiance = light.colorAndIntensity.rgb *
            light.colorAndIntensity.w * attenuation;
        directLighting += (diffuseWeight * albedo / PI + specular) *
            radiance * nDotL * visibility;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 reflectDirView = reflect(-V, N);
    vec3 reflectDirWorld = viewToWorld * reflectDirView;
    vec3 envReflection = texture(cubemap, reflectDirWorld).rgb;
    vec3 environmentFresnel = FresnelSchlick(nDotV, f0);
    vec3 environmentSpecular = envReflection * environmentFresnel *
        material.environmentReflectivity * (1.0 - roughness * 0.75);

    color = vec4(
        ambient + directLighting + environmentSpecular,
        clamp(material.opacity * albedoSample.a, 0.0, 1.0));
}
