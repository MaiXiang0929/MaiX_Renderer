// SPDX-License-Identifier: MIT
// Split-sum IBL, following LearnOpenGL's PBR/IBL reference shaders.
// https://github.com/JoeyDeVries/LearnOpenGL/tree/a545a703f95893258d16dbe32f5ccbb6400fd213/src/6.pbr/2.2.2.ibl_specular_textured
#version 400 core
in vec2 fragTexCoord;
out vec4 color;
uniform samplerCube environmentMap;
uniform int mode; // 0: diffuse E/pi, 1: GGX prefilter, 2: integrated BRDF
uniform int face;
uniform float roughness;
uniform float sourceResolution;
const float PI = 3.14159265359;
const uint Samples = 512u;

vec2 SamplePoint(uint i)
{
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xaaaaaaaau) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xccccccccu) >> 2u);
    bits = ((bits & 0x0f0f0f0fu) << 4u) | ((bits & 0xf0f0f0f0u) >> 4u);
    bits = ((bits & 0x00ff00ffu) << 8u) | ((bits & 0xff00ff00u) >> 8u);
    return vec2(float(i) / float(Samples), float(bits) * 2.3283064365386963e-10);
}
mat3 Basis(vec3 n)
{
    vec3 axis = abs(n.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 t = normalize(cross(axis,n));
    return mat3(t,cross(n,t),n);
}
vec3 GGXHalfVector(vec2 xi, float r)
{
    float alpha = r*r;
    float c = sqrt((1.0-xi.y) / max(1.0+(alpha*alpha-1.0)*xi.y,1e-7));
    float s = sqrt(max(1.0-c*c,0.0));
    float phi = 2.0*PI*xi.x;
    return vec3(cos(phi)*s,sin(phi)*s,c);
}
vec3 FaceDirection()
{
    vec2 p = fragTexCoord*2.0-1.0;
    if (face==0) return normalize(vec3(1,-p.y,-p.x));
    if (face==1) return normalize(vec3(-1,-p.y,p.x));
    if (face==2) return normalize(vec3(p.x,1,p.y));
    if (face==3) return normalize(vec3(p.x,-1,-p.y));
    if (face==4) return normalize(vec3(p.x,-p.y,1));
    return normalize(vec3(-p.x,-p.y,-1));
}
vec2 IntegrateBRDF(vec2 uv)
{
    float nv = max(uv.x,1e-4);
    float r = uv.y;
    vec3 v = vec3(sqrt(max(1.0-nv*nv,0.0)),0,nv);
    float k = r*r*0.5; // IBL geometry, different from direct lighting
    float gv = nv / (nv*(1.0-k)+k);
    vec2 integral = vec2(0);
    for (uint i=0u;i<Samples;++i)
    {
        vec3 h = GGXHalfVector(SamplePoint(i),r);
        float vh = max(dot(v,h),0.0);
        vec3 l = 2.0*vh*h-v;
        float nl = max(l.z,0.0);
        if (nl>0.0)
        {
            float gl = nl / (nl*(1.0-k)+k);
            float visibility = gv*gl*vh / max(h.z*nv,1e-5);
            float fc = pow(1.0-vh,5.0);
            integral += vec2(1.0-fc,fc)*visibility;
        }
    }
    return integral/float(Samples);
}
void main()
{
    if (mode==2) { color=vec4(IntegrateBRDF(fragTexCoord),0,1); return; }
    vec3 n = FaceDirection();
    if (mode==1 && roughness<0.001)
    { color=vec4(textureLod(environmentMap,n,0).rgb,1); return; }
    mat3 basis = Basis(n);
    vec3 sum = vec3(0);
    float weight = 0.0;
    float texelArea = 4.0*PI / (6.0*sourceResolution*sourceResolution);
    for (uint i=0u;i<Samples;++i)
    {
        vec2 xi = SamplePoint(i);
        if (mode==0)
        {
            // Cosine-weighted sampling stores E/pi, ready to multiply by albedo.
            float radial = sqrt(xi.y);
            vec3 l = basis*vec3(radial*cos(2.0*PI*xi.x),radial*sin(2.0*PI*xi.x),sqrt(1.0-xi.y));
            float pdf = max(sqrt(1.0-xi.y)/PI,1e-5);
            float lod = max(0.5*log2(1.0/(float(Samples)*pdf*texelArea)),0.0);
            sum += textureLod(environmentMap,l,lod).rgb;
            weight += 1.0;
        }
        else
        {
            vec3 h = basis*GGXHalfVector(xi,roughness);
            float nh = max(dot(n,h),0.0);
            vec3 l = normalize(2.0*nh*h-n);
            float nl = max(dot(n,l),0.0);
            if (nl>0.0)
            {
                float a2 = pow(roughness,4.0);
                float denominator = nh*nh*(a2-1.0)+1.0;
                float distribution = a2 / max(PI*denominator*denominator,1e-7);
                float pdf = max(distribution*0.25,1e-5); // V=N, hence N.H == V.H
                float lod = max(0.5*log2(1.0/(float(Samples)*pdf*texelArea)),0.0);
                sum += textureLod(environmentMap,l,lod).rgb*nl;
                weight += nl;
            }
        }
    }
    color=vec4(sum/max(weight,1e-5),1);
}
