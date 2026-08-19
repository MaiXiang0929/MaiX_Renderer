#version 400 core

layout(triangles, equal_spacing, ccw) in;

in TC_OUT
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
} teIn[];

out vec3 fragPos;
out vec3 fragNormal;
out vec3 fragTangent;
out float fragTangentSign;
out vec2 fragTexCoord;
out vec4 fragLightSpacePos;
out vec3 fragFaceForward;
out vec3 fragFaceRight;

uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 lightMvp;
uniform sampler2D displacementMap;
uniform bool hasDisplacementMap;
uniform float displacementScale;
uniform vec3 faceForwardLocal;
uniform vec3 faceRightLocal;

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
    vec4 tangent =
        weights.x * teIn[0].tangent +
        weights.y * teIn[1].tangent +
        weights.z * teIn[2].tangent;

    if (hasDisplacementMap)
        position += normal * texture(displacementMap, uv).r * displacementScale;

    gl_Position = mvp * vec4(position, 1.0);
    fragPos = vec3(mv * vec4(position, 1.0));

    mat3 normalMatrix = transpose(inverse(mat3(mv)));
    fragNormal = normalize(normalMatrix * normal);
    fragTangent = normalize(mat3(mv) * tangent.xyz);
    fragTangentSign = tangent.w < 0.0 ? -1.0 : 1.0;
    fragTexCoord = uv;
    fragLightSpacePos = lightMvp * vec4(position, 1.0);

    fragFaceForward = normalize(mat3(mv) * faceForwardLocal);
    vec3 faceRightView = mat3(mv) * faceRightLocal;
    faceRightView -= fragFaceForward * dot(faceRightView, fragFaceForward);
    fragFaceRight = normalize(faceRightView);
}
