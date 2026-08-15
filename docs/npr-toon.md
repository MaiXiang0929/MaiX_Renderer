# Minimal NPR Toon Path

Materials now select either the existing Cook-Torrance `PBR` path or a minimal
`Toon` path through `MaterialProperties::shadingModel`. The Forward fragment
shader keeps the same vertex attributes, light UBO, shadow map, and material
texture bindings for both modes.

The Toon branch computes a per-light diffuse band from `N dot L`, applies the
existing distance, spot, and shadow visibility terms, and adds an artist-facing
Rim Light from `pow(1 - N dot V, 3)`. It intentionally skips PBR ORM and
specular evaluation after resolving the albedo and normal.

The Material Editor exposes the shading model, threshold, shadow color and
strength, and rim parameters. Renderer clamps these values before writing the
owning material, so all Forward, Reflection, and Translucency submissions see
the same update on the next frame.

This milestone does not include outlines, face shadow textures, hair highlight,
or a separate NPR pass.
