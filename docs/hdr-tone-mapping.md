# HDR Scene Color and Tone Mapping

## Pipeline position

The Forward color target uses `RGBA16F`. Opaque surfaces, translucent surfaces,
the skybox, the reflective ground, and editor primitives therefore accumulate
into one linear HDR scene-color texture before presentation.

```text
sRGB material and cubemap textures
    -> hardware sRGB decode
    -> linear PBR lighting and blending
    -> Forward RGBA16F scene color
    -> half-resolution HDR Bloom
    -> PostProcess exposure compensation (2^EV)
    -> PostProcess ACES fitted tone mapping
    -> linear-to-sRGB encoding
    -> default window framebuffer
    -> ImGui overlay
```

CPU responsibilities are color-format allocation and submission of the
artist-facing settings. The GPU composites Bloom, then performs exposure, tone
mapping, and output encoding once per pixel in `postprocess.frag`. `PresentPass`
only copies the display-ready texture to the default framebuffer.

## Color spaces

- Albedo and cubemap images are stored as sRGB textures and decode to linear
  values when sampled.
- Normal, specular, displacement, shadow, and render-target textures remain
  linear data.
- The Forward target stores linear `RGBA16F` values and can preserve highlights
  above `1.0` until Present.
- Reflection remains half-resolution `RGBA8` for now. It is a secondary linear
  input to the reflective ground and does not receive display encoding.
- ImGui renders after Present directly to the default framebuffer, so editor UI
  colors are not affected by scene exposure or tone mapping.

## Artist controls

`Renderer Statistics` exposes:

- `Tone mapping`: switches between ACES fitted compression and a diagnostic
  clamp-to-display-range path.
- `Exposure (EV)`: ranges from `-8` to `+8`; each positive stop doubles scene
  luminance and each negative stop halves it before tone mapping.

Exposure is manual. Automatic exposure requires a separate luminance reduction
or histogram pass and temporal adaptation, which are outside this stage.

## Performance and memory

At 1920x1080, an `RGBA16F` color surface occupies roughly 15.8 MiB before mip
levels, twice the storage of `RGBA8`. Bloom adds three half-resolution HDR
working targets. PostProcess adds one full-resolution `RGBA8` display target.
No CPU readback or GPU synchronization is introduced.
