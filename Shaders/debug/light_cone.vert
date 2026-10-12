#version 400 core

layout(location = 0) in vec3 localPosition;

uniform mat4 viewProjection;
uniform vec3 lightPosition;
uniform vec3 lightDirection;
uniform float lightRange;
uniform float outerConeAngle;

void main()
{
    vec3 forward = normalize(lightDirection);
    vec3 helper = abs(forward.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(helper, forward));
    vec3 up = cross(forward, right);
    float radius = tan(outerConeAngle) * lightRange;
    vec3 worldPosition = lightPosition +
        right * localPosition.x * radius +
        up * localPosition.y * radius +
        forward * localPosition.z * lightRange;
    gl_Position = viewProjection * vec4(worldPosition, 1.0);
}
