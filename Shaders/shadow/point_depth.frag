#version 400 core
in vec2 fragTexCoord;
in vec3 shadowWorldPosition;
uniform bool coverageMasked;
uniform float coverageOpacity;
uniform float coverageCutoff;
uniform bool coverageHasMap;
uniform sampler2D coverageMap;
uniform vec3 shadowPosition;
uniform float shadowNear;
uniform float shadowFar;
void main()
{
    if (coverageMasked && coverageOpacity * (coverageHasMap ? texture(coverageMap, fragTexCoord).a : 1.0) < coverageCutoff)
        discard;
    float distanceToLight = length(shadowWorldPosition - shadowPosition);
    if (distanceToLight < shadowNear || distanceToLight >= shadowFar) discard;
    gl_FragDepth = (distanceToLight - shadowNear) / (shadowFar - shadowNear);
}
