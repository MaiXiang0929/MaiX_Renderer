# NPR Inverted Hull Outline

The minimal NPR outline is a dedicated pass between Forward and Translucency:

```text
Forward opaque -> Outline -> Translucency -> Bloom -> PostProcess
```

Only opaque Toon materials with `outlineEnabled` participate. The pass binds
the existing Forward HDR framebuffer without clearing it, reuses the opaque
depth buffer, and submits the main view's existing `RenderItem` meshes. It does
not allocate another color or depth target.

## CPU and GPU responsibilities

The CPU filters render items and reads `outlineThickness` and `outlineColor`
from the Renderer-owned material. For each item it computes `modelView` and the
inverse-transpose 3x3 normal matrix, then submits those uniforms and one mesh
draw.

The vertex shader transforms positions and normals into view space and expands
the hull along the normalized view-space normal. The fragment shader converts
the artist-facing color to linear space before writing it into RGBA16F Scene
Color. Front-face culling leaves the enlarged back faces visible around the
original silhouette. Depth testing remains enabled, depth writes are disabled,
and blending is disabled. The pass restores all modified fixed-function state.

The resulting outline is part of Scene Color, so it follows the same Bloom,
exposure, ACES tone mapping, and output encoding path as the opaque surface.
Translucent objects are composited afterward against the original opaque depth.

## Artist controls

The Material Editor exposes Outline, Outline Thickness, and Outline Color only
for Toon materials. Thickness is measured in view-space scene units and is
clamped to `[0.0, 0.2]`; this makes distant outlines naturally thinner on
screen. Renderer validation remains the only mutation boundary for material
resources.

## Deliberate limits

This milestone does not outline translucent materials or reflection views and
does not batch outline draws. Tessellation disables the pass because the base
mesh cannot reproduce displaced silhouettes. Screen-space constant-pixel
width, crease lines, face shadow textures, and hair highlights remain separate
future work.
