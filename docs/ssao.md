# Screen-Space Ambient Occlusion

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

This is a display-oriented approximation: the composite scales accumulated
scene color, so direct light, reflections, and translucent color are affected
together. Separating AO to indirect light requires additional lighting buffers
or a deferred/G-buffer path and is outside this implementation.
