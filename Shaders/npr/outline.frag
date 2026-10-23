#version 400 core
in vec2 fragTexCoord;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 indirectDiffuse;

uniform vec3 outlineColor;


uniform bool coverageMasked;
uniform float coverageOpacity;
uniform float coverageCutoff;
uniform bool coverageHasMap;
uniform sampler2D coverageMap;

void main()
{
    indirectDiffuse = vec4(0.0);
    if (coverageMasked && coverageOpacity * (coverageHasMap ? texture(coverageMap, fragTexCoord).a : 1.0) < coverageCutoff)
        discard;

    vec3 linearColor = pow(max(outlineColor, vec3(0.0)), vec3(2.2));
    color = vec4(linearColor, 1.0);
}
