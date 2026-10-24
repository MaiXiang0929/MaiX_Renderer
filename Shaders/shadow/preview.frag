#version 400 core
in vec2 fragTexCoord;
out vec4 color;
uniform sampler2DArrayShadow depthArray;
uniform samplerCubeArrayShadow depthCubes;
uniform bool point;
uniform int slice;
uniform int layer;
vec3 FaceDirection(vec2 uv)
{
    vec2 q = uv*2.0-1.0;
    if (slice==0) return vec3(1,-q.y,-q.x);
    if (slice==1) return vec3(-1,-q.y,q.x);
    if (slice==2) return vec3(q.x,1,q.y);
    if (slice==3) return vec3(q.x,-1,-q.y);
    if (slice==4) return vec3(q.x,-q.y,1);
    return vec3(-q.x,-q.y,-1);
}
void main()
{
    // Invert the depth comparison without mutating the shared sampler state.
    float low=0.0, high=1.0;
    for (int i=0;i<14;++i) {
        float middle=(low+high)*0.5;
        float visibility=point ? texture(depthCubes,vec4(FaceDirection(fragTexCoord),float(layer)),middle)
            : texture(depthArray,vec4(fragTexCoord,float(layer),middle));
        if (visibility>0.5) low=middle; else high=middle;
    }
    color=vec4(vec3((low+high)*0.5),1.0);
}
