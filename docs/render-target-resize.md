# Render Target Resize

## Ownership and data flow

`Application` owns the GLFW framebuffer size and writes it to
`RenderFrameData::viewportWidth/viewportHeight`. It does not resize OpenGL
resources directly.

Before executing any pass, `RenderPipeline` compares that extent with the
targets owned by Forward, Reflection, Bloom, SSAO, PostProcess, Present, and
EditorPrimitive passes:

```text
GLFW framebuffer callback (CPU)
    -> Application camera aspect ratios (CPU)
    -> RenderFrameData viewport extent (CPU)
    -> RenderPipeline extent comparison (CPU)
    -> candidate Framebuffer color/depth attachment creation (GPU resource)
    -> completeness check and commit, or discard candidate on failure
    -> each pass binds its target and viewport (GPU state)
    -> PresentPass restores the window viewport
```

Forward, PostProcess, Present, SSAO composite, and EditorPrimitive targets use
the full viewport extent. Reflection, Bloom, and SSAO working targets use half
resolution in each dimension, rounded up for odd dimensions. Forward scene
color and SSAO composite use `RGBA16F`; Reflection, Present, and editor overlay
use `RGBA8`; SSAO working targets use `R8`.

## Lifetime rules

- Targets are created lazily on the first frame with a valid non-zero extent.
- A target is recreated only when its required extent changes.
- A zero width or height represents a minimized window. The pipeline skips the
  frame without deleting the last valid GPU resources.
- `Framebuffer::Init` rejects zero dimensions. It creates candidate color and
  depth attachments, checks FBO completeness, and only then replaces the old
  target. A failed resize preserves the previous texture IDs, extent, format,
  and complete FBO. It also restores OpenGL bindings; a successful replacement
  remaps bindings of deleted resources to their replacements.
- If any target resize fails, the pipeline skips the pass sequence for that
  frame and retries on the next frame. Bloom and SSAO compare every internal
  target, so a partial resize cannot be mistaken for a complete one.
- Main and reflection views share the main camera projection, so both targets
  keep the same aspect ratio. The presentation plane is scaled to that aspect
  before projection to avoid distorting the rendered texture.

## Current limitation

Resize recreates attachments immediately on the render thread. Continuous
window dragging can therefore cause repeated allocations. This is acceptable
for the current renderer; a future render graph can pool targets or debounce
resize events if profiling shows allocation stalls.

`FramebufferTests` creates a hidden OpenGL context, forces an incomplete
oversized target, verifies that the previous target and bindings survive, and
then verifies a successful replacement. The test prints `skipped` when an
OpenGL 4.0 context is unavailable; check its output when validating a graphics
machine.
