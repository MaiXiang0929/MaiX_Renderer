#version 330 core
in vec2 fragTexCoord;
layout(location = 0) out float color;
uniform sampler2D depthTexture;
uniform mat4 inverseProjection;
uniform vec3 depthTexelSize;
uniform float sampleRadius;
uniform float bias;

vec3 Position(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = inverseProjection * clip;
    return view.xyz / max(view.w, 0.000001);
}

void main()
{
    float depth = texture(depthTexture, fragTexCoord).r;
    if (depth >= 0.99999) { color = 1.0; return; }
    vec3 position = Position(fragTexCoord, depth);
    vec3 dx = Position(fragTexCoord + vec2(depthTexelSize.x, 0.0), texture(depthTexture, fragTexCoord + vec2(depthTexelSize.x, 0.0)).r) - position;
    vec3 dy = Position(fragTexCoord + vec2(0.0, depthTexelSize.y), texture(depthTexture, fragTexCoord + vec2(0.0, depthTexelSize.y)).r) - position;
    vec3 normal = normalize(cross(dx, dy));
    vec3 toCamera = normalize(-position);
    if (dot(normal, toCamera) < 0.0) normal = -normal;
    vec2 offsets[8] = vec2[8](vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),vec2(0.707,0.707),vec2(-0.707,0.707),vec2(0.707,-0.707),vec2(-0.707,-0.707));
    float occlusion = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        vec2 uv = fragTexCoord + offsets[i] * depthTexelSize.xy * 8.0;
        float sampleDepth = texture(depthTexture, uv).r;
        if (sampleDepth >= 0.99999) continue;
        vec3 samplePosition = Position(uv, sampleDepth);
        vec3 delta = samplePosition - position;
        float distanceToSample = length(delta);
        if (distanceToSample <= 0.00001 || distanceToSample > sampleRadius) continue;
        float facing = max(dot(normal, delta / distanceToSample), 0.0);
        float behind = step(position.z + bias, samplePosition.z);
        float rangeWeight = 1.0 - smoothstep(0.0, sampleRadius, distanceToSample);
        occlusion += behind * facing * rangeWeight;
    }
    color = 1.0 - clamp(occlusion / 8.0, 0.0, 1.0);
}
