#version 400 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

uniform mat4 mvp;
uniform mat4 mv;
uniform int surfaceSide;
uniform vec3 faceForwardLocal;
uniform vec3 faceRightLocal;

out vec3 fragPos;
out vec3 fragNormal;
out vec3 fragTangent;
out float fragTangentSign;
out vec2 fragTexCoord;
out vec3 fragFaceForward;
out vec3 fragFaceRight;

void main()
{
    gl_Position = mvp * vec4(pos, 1.0);
    fragPos = vec3(mv * vec4(pos, 1.0));

    mat3 normalMatrix = transpose(inverse(mat3(mv)));
    fragNormal = normalize(normalMatrix * normal);
    fragTangent = normalize(mat3(mv) * tangent.xyz);
    fragTangentSign = tangent.w;
    // A reflection of either the model or view reverses the tangent frame.
    // Legacy keeps its historical orientation, including mirrored transforms.
    if (surfaceSide != 3)
        fragTangentSign *= sign(determinant(mat3(mv)));
    fragTexCoord = texCoord;

    fragFaceForward = normalize(mat3(mv) * faceForwardLocal);
    vec3 faceRightView = mat3(mv) * faceRightLocal;
    faceRightView -= fragFaceForward * dot(faceRightView, fragFaceForward);
    fragFaceRight = normalize(faceRightView);
}
