#version 400 core
in vec2 fragTexCoord;


uniform bool coverageMasked;
uniform float coverageOpacity;
uniform float coverageCutoff;
uniform bool coverageHasMap;
uniform sampler2D coverageMap;

void main()
{
    if (coverageMasked && coverageOpacity * (coverageHasMap ? texture(coverageMap, fragTexCoord).a : 1.0) < coverageCutoff)
        discard;

}
