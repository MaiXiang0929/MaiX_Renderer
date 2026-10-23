// SPDX-License-Identifier: MIT
#version 400 core
in vec2 fragTexCoord;
layout(location = 0) out vec4 color;
uniform sampler2D panorama;
uniform int face;
const float PI = 3.14159265359;

vec3 FaceDirection()
{
    // Same OpenGL face orientation as ibl_bake.frag.
    vec2 p = fragTexCoord * 2.0 - 1.0;
    if (face == 0) return normalize(vec3(1,-p.y,-p.x));
    if (face == 1) return normalize(vec3(-1,-p.y,p.x));
    if (face == 2) return normalize(vec3(p.x,1,p.y));
    if (face == 3) return normalize(vec3(p.x,-1,-p.y));
    if (face == 4) return normalize(vec3(p.x,-p.y,1));
    return normalize(vec3(-p.x,-p.y,-1));
}
vec3 PanoramaPixel(ivec2 pixel, ivec2 size)
{
    // Longitude wraps; latitude clamps at the poles. No RHI sampler extension is needed.
    pixel.x = (pixel.x % size.x + size.x) % size.x;
    pixel.y = clamp(pixel.y, 0, size.y - 1);
    return texelFetch(panorama, pixel, 0).rgb;
}
void main()
{
    vec3 d = FaceDirection();
    // CPU row zero is north. Positive X is the center longitude, positive Z is U=0.75.
    vec2 uv = vec2(atan(d.z,d.x) / (2.0*PI) + 0.5, 0.5 - asin(clamp(d.y,-1.0,1.0)) / PI);
    ivec2 size = textureSize(panorama,0);
    vec2 pixel = uv * vec2(size) - 0.5;
    ivec2 base = ivec2(floor(pixel)); vec2 f = fract(pixel);
    vec3 low = mix(PanoramaPixel(base,size),PanoramaPixel(base+ivec2(1,0),size),f.x);
    vec3 high = mix(PanoramaPixel(base+ivec2(0,1),size),PanoramaPixel(base+ivec2(1,1),size),f.x);
    color = vec4(mix(low,high,f.y),1.0);
}
