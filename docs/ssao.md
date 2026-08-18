# Screen-Space Ambient Occlusion

Status: implemented; visual acceptance pending.

Last verified: 2026-08-18

The SSAO path is an opt-in post-process stage between Translucency and Bloom.
Forward writes full-resolution RGBA16F scene color and sampleable depth. SSAO
then writes half-resolution raw and filtered AO in R8 targets, followed by a
full-resolution RGBA16F composite. Bloom, exposure, tone mapping, and Present
continue to consume the full-resolution HDR scene color.

The CPU owns the targets and submits the inverse projection plus artist-facing
radius, bias, and intensity parameters. The GPU reconstructs view-space
positions from depth, estimates a normal from neighboring positions, evaluates
eight samples, performs a depth-aware 3x3 filter, and composites the AO term.
Background depth returns an AO value of one and depth discontinuities are
rejected by the filter to limit contour halos.

All reconstructed positions and estimated normals stay in view space. The
cross-product normal is oriented into the camera-facing hemisphere with the
direction from the reconstructed surface position to the view-space origin;
this avoids treating a fixed view-space Z sign as a surface-orientation test.

Artist intensity scales occlusion rather than extrapolating between one and
the AO sample. The composite uses
`1 - clamp((1 - clamp(ao, 0, 1)) * clamp(intensity, 0, 3), 0, 1)`, so its
multiplier remains within `[0, 1]` throughout the supported intensity range.
The sampleable `GL_DEPTH24_STENCIL8` attachment relies on the OpenGL 4.0 depth
sampling behavior. Selecting a stencil texture mode is intentionally omitted
because `GL_DEPTH_STENCIL_TEXTURE_MODE` is not core until OpenGL 4.3.

This is a display-oriented approximation: the composite scales accumulated
scene color, so direct light, reflections, and translucent color are affected
together. Separating AO to indirect light requires additional lighting buffers
or a deferred/G-buffer path and is outside this implementation.

## Validation

Launch `OpenGL_Project.exe --material-lab`, open `Renderer Statistics`, and
enable SSAO. An enabled frame should report three SSAO fullscreen draws and a
non-zero SSAO GPU time after timer-query results become available. Check the
material contact areas and concavities, background pixels, silhouette halos,
camera motion, the full intensity range, and framebuffer resize behavior.

Build, startup, framebuffer creation, and shader compile/link errors are
verified by the implementation workflow. The final image remains subject to
user visual acceptance.
