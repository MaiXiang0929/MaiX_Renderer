// 反射地面片段着色器
// 使用 render-to-texture 的反射纹理 + cubemap 实现地面反射

#version 330 core

in vec3 worldPos;

uniform samplerCube cubemap;
uniform sampler2D reflectionTex;
uniform mat4 reflectionVP;
uniform vec3 cameraWorldPos;
uniform mat4 lightVP;
uniform sampler2DShadow shadowMap;
uniform bool shadowsEnabled;
uniform bool shadowIsDirectional;

float CalculateShadowVisibility(vec4 lightSpacePos)
{
    // 关闭时地面保持原有反射亮度，并跳过 PCF 采样。
    if (!shadowsEnabled) {
        return 1.0;
    }

    if (lightSpacePos.w <= 0.0) {
        return shadowIsDirectional ? 1.0 : 0.0;
    }

    vec3 projected = lightSpacePos.xyz / lightSpacePos.w;
    projected = projected * 0.5 + 0.5;

    // 方向光在阴影图外保留直射；聚光灯沿用光锥外遮蔽约定。
    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0 ||
        projected.z < 0.0 || projected.z > 1.0) {
        return shadowIsDirectional ? 1.0 : 0.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            visibility += texture(
                shadowMap,
                vec3(projected.xy + vec2(x, y) * texelSize,
                     projected.z - 0.001));
        }
    }
    return visibility / 9.0;
}

layout(location = 0) out vec4 color;

void main()
{
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
        result = texture(cubemap, R).rgb;
    }

    // 地面不写入 shadow map，只在此处接收 场景模型投下的阴影。
    float visibility = CalculateShadowVisibility(lightVP * vec4(worldPos, 1.0));
    result *= mix(0.35, 1.0, visibility);

    color = vec4(result, 1.0);
}
