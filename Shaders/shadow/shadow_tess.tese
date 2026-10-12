#version 400 core

layout(triangles, equal_spacing, ccw) in;

in TC_OUT
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
} teIn[];

uniform mat4 lightMvp;
uniform sampler2D displacementMap;
uniform bool hasDisplacementMap;
uniform float displacementScale;

void main()
{
    vec3 weights = gl_TessCoord.xyz;
    vec3 position =
        weights.x * teIn[0].position +
        weights.y * teIn[1].position +
        weights.z * teIn[2].position;
    vec3 normal = normalize(
        weights.x * teIn[0].normal +
        weights.y * teIn[1].normal +
        weights.z * teIn[2].normal);
    vec2 uv =
        weights.x * teIn[0].uv +
        weights.y * teIn[1].uv +
        weights.z * teIn[2].uv;

    if (hasDisplacementMap)
        position += normal * texture(displacementMap, uv).r * displacementScale;

    gl_Position = lightMvp * vec4(position, 1.0);
}
