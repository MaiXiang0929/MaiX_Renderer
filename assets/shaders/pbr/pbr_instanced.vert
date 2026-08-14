#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(std140) uniform InstanceTransforms
{
    mat4 instanceModelViews[256];
};

uniform mat4 projectionFromView;
uniform mat4 lightFromView;

out vec3 fragPos;
out vec3 fragNormal;
out vec3 fragTangent;
out float fragTangentSign;
out vec2 fragTexCoord;
out vec4 fragLightSpacePos;

void main()
{
    mat4 modelView = instanceModelViews[gl_InstanceID];
    vec4 viewPosition = modelView * vec4(pos, 1.0);
    gl_Position = projectionFromView * viewPosition;
    fragPos = viewPosition.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelView)));
    fragNormal = normalize(normalMatrix * normal);
    fragTangent = normalize(mat3(modelView) * tangent.xyz);
    fragTangentSign = tangent.w;
    fragTexCoord = texCoord;
    fragLightSpacePos = lightFromView * viewPosition;
}
