# Bloom and Post-Process Pipeline

## Pipeline position

The main view now keeps display mapping separate from presentation:

```text
Forward / Translucency
    -> RGBA16F linear HDR Scene Color
    -> BloomPass: half-resolution highlight extraction and separable blur
    -> PostProcessPass: HDR bloom composite, exposure, ACES, sRGB encoding
    -> RGBA8 display color
    -> PresentPass: composite with the independent Editor Overlay Buffer
    -> ImGui
```

Bloom is composited before exposure and tone mapping. This preserves HDR energy
and prevents the display transform from destroying highlight values needed by
the filter. `PresentPass` no longer performs color grading or tone mapping.

## CPU and GPU responsibilities

The CPU owns three half-resolution `RGBA16F` bloom targets and one
full-resolution `RGBA8` post-process target through `RenderPipeline`. Resize
recreates them with the Forward and Reflection targets. The CPU also clamps and
submits the artist-facing enable, threshold, intensity, exposure, and tone-map
settings.

The GPU extracts pixels above the threshold, performs eight alternating
horizontal and vertical five-tap blur draws, composites the result with HDR
Scene Color, and applies the display transform. No CPU readback or synchronous
GPU query is introduced.

Editor primitives render after PostProcess into a separate display-space
Overlay Buffer. They are not sampled by Bloom and are not modified by exposure
or tone mapping. Present is the only scene/overlay composition point.

## Artist controls

`Renderer Statistics` exposes:

- `Bloom`: skips extraction and blur draws when disabled.
- `Bloom threshold`: selects the HDR brightness that begins contributing.
- `Bloom intensity`: scales the blurred HDR contribution before exposure.
- Existing exposure and tone-mapping controls continue to affect the combined
  HDR result.

The initial implementation intentionally omits multi-resolution mip-chain
bloom, soft-knee controls, lens dirt, starbursts, automatic exposure, SSAO, and
render-target pooling.
