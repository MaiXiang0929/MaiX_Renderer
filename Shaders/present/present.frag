#version 330 core

/*
 * FXAA adapted from three.js r180 examples/jsm/shaders/FXAAShader.js,
 * commit 0af9729d0c143a86a1d725d6e2c3ad83301f3f34.
 * NVIDIA FXAA; C# implementation by Jasper Flick, GLSL port by Dave Hoskins.
 * https://github.com/mrdoob/three.js/blob/r180/examples/jsm/shaders/FXAAShader.js
 *
 * The MIT License
 * Copyright (c) 2010-2025 three.js authors
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

in vec2 fragTexCoord;
layout(location = 0) out vec4 color;
uniform sampler2D renderedTexture;
uniform sampler2D editorOverlayTexture;
uniform bool fxaaEnabled;

// The input is display-referred, sRGB-encoded RGB stored in an ordinary RGBA8 texture.
// Explicit LOD 0 avoids derivative-dependent mip selection during divergent edge searches.
vec3 SceneSample(vec2 uv)
{
    return textureLod(renderedTexture, uv, 0.0).rgb;
}

float Luminance(vec2 uv)
{
    return dot(SceneSample(uv), vec3(0.3, 0.59, 0.11));
}

vec3 ApplyFxaa(vec2 uv)
{
    vec2 texel = 1.0 / vec2(textureSize(renderedTexture, 0));
    float center = Luminance(uv);
    float north = Luminance(uv + vec2(0.0, texel.y));
    float south = Luminance(uv - vec2(0.0, texel.y));
    float east = Luminance(uv + vec2(texel.x, 0.0));
    float west = Luminance(uv - vec2(texel.x, 0.0));
    float low = min(center, min(min(north, south), min(east, west)));
    float high = max(center, max(max(north, south), max(east, west)));
    float contrast = high - low;
    if (contrast < max(0.0312, high * 0.063))
        return SceneSample(uv);

    float ne = Luminance(uv + texel);
    float nw = Luminance(uv + vec2(-texel.x, texel.y));
    float se = Luminance(uv + vec2(texel.x, -texel.y));
    float sw = Luminance(uv - texel);
    float horizontal = 2.0 * abs(north + south - 2.0 * center)
        + abs(ne + se - 2.0 * east) + abs(nw + sw - 2.0 * west);
    float vertical = 2.0 * abs(east + west - 2.0 * center)
        + abs(ne + nw - 2.0 * north) + abs(se + sw - 2.0 * south);
    bool isHorizontal = horizontal >= vertical;
    float positive = isHorizontal ? north : east;
    float negative = isHorizontal ? south : west;
    float positiveGradient = abs(positive - center);
    float negativeGradient = abs(negative - center);
    float pixelStep = isHorizontal ? texel.y : texel.x;
    float opposite = positive;
    float gradient = positiveGradient;
    if (negativeGradient > positiveGradient)
    {
        pixelStep = -pixelStep;
        opposite = negative;
        gradient = negativeGradient;
    }

    // Search both directions along the edge, bounded to six samples per side.
    vec2 across = isHorizontal ? vec2(0.0, pixelStep) : vec2(pixelStep, 0.0);
    vec2 along = isHorizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    vec2 edgeUv = uv + across * 0.5;
    float edgeLuma = (center + opposite) * 0.5;
    float threshold = gradient * 0.25;
    const float steps[6] = float[6](1.0, 1.5, 2.0, 2.0, 2.0, 4.0);
    vec2 forwardUv = edgeUv + along * steps[0];
    vec2 backwardUv = edgeUv - along * steps[0];
    float forwardDelta = Luminance(forwardUv) - edgeLuma;
    float backwardDelta = Luminance(backwardUv) - edgeLuma;
    bool forwardDone = abs(forwardDelta) >= threshold;
    bool backwardDone = abs(backwardDelta) >= threshold;
    for (int i = 1; i < 6; ++i)
    {
        if (!forwardDone)
        {
            forwardUv += along * steps[i];
            forwardDelta = Luminance(forwardUv) - edgeLuma;
            forwardDone = abs(forwardDelta) >= threshold;
        }
        if (!backwardDone)
        {
            backwardUv -= along * steps[i];
            backwardDelta = Luminance(backwardUv) - edgeLuma;
            backwardDone = abs(backwardDelta) >= threshold;
        }
        if (forwardDone && backwardDone) break;
    }
    if (!forwardDone) forwardUv += along * 8.0;
    if (!backwardDone) backwardUv -= along * 8.0;
    float forwardDistance = isHorizontal ? forwardUv.x - uv.x : forwardUv.y - uv.y;
    float backwardDistance = isHorizontal ? uv.x - backwardUv.x : uv.y - backwardUv.y;
    bool nearerForward = forwardDistance <= backwardDistance;
    float nearestDelta = nearerForward ? forwardDelta : backwardDelta;
    float nearestDistance = min(forwardDistance, backwardDistance);
    float edgeBlend = 0.0;
    if ((nearestDelta >= 0.0) != (center - edgeLuma >= 0.0))
        edgeBlend = 0.5 - nearestDistance / (forwardDistance + backwardDistance);

    float average = (2.0 * (north + south + east + west) + ne + nw + se + sw) / 12.0;
    float subpixelBlend = smoothstep(0.0, 1.0, clamp(abs(average - center) / contrast, 0.0, 1.0));
    subpixelBlend *= subpixelBlend;
    return SceneSample(uv + across * max(subpixelBlend, edgeBlend));
}

void main()
{
    vec3 sceneColor = fxaaEnabled ? ApplyFxaa(fragTexCoord) : texture(renderedTexture, fragTexCoord).rgb;
    // Filter the scene alone. Editor overlays and the later ImGui UI retain their original edges.
    vec4 overlay = texture(editorOverlayTexture, fragTexCoord);
    color = vec4(overlay.rgb + sceneColor * (1.0 - overlay.a), 1.0);
}
