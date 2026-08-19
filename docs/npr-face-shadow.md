# Minimal Toon Face Shadow

## Demo Goal

This feature fills the Face Shadow shot in the `25-40s NPR` section of the
60-second demo. It extends the existing Toon material path and does not add a
render pass, framebuffer, or draw call.

## Texture Contract

`MaterialTextureSlot::FaceShadow` is an RGBA8 linear-data texture. The Forward
shader reads the red channel as a per-pixel light-angle threshold:

- `0` keeps the pixel in shadow except when the key light faces the character.
- `1` allows the pixel to remain lit across the full horizontal light arc.
- Face Shadow uses the Base Color UV set; no additional vertex attribute is
  required.
- The authored texture represents light arriving from local `+Face Right`.
  Light from the opposite side mirrors U automatically.
- `Mirror Face Shadow X` adapts an asset whose UV or left/right convention is
  reversed.

The slot uses texture unit 6. Units 0-3 remain material PBR inputs, unit 4 is
the cubemap, and unit 5 is the 2D shadow map.

## Coordinate Spaces And Data Flow

CPU responsibilities:

1. `Application` submits an explicit `keyLightId`, independent of the light
   that owns the 2D shadow map.
2. `BuildLightUploadData` maps both ids to their indices in the truncated
   Forward light array.
3. `Material` owns the Face Shadow texture and binds artist parameters through
   the existing Renderer-owned material update boundary.
4. Renderer normalizes Face Forward and orthogonalizes Face Right before
   storing a material edit.

GPU responsibilities:

1. Standard, instanced, and tessellated vertex paths transform local Face
   Forward and Right into view space using the current model-view matrix.
2. The fragment shader projects the view-space key-light direction onto that
   face frame.
3. It converts the absolute horizontal angle to `[0, 1]`, selects the authored
   or mirrored UV, and compares the angle against the texture red channel.
4. `Face Shadow Softness` widens that comparison with `smoothstep`.
5. The result replaces only the key light's Toon diffuse band. Existing
   shadow-map visibility, light color/intensity, Rim Light, opacity, and all
   non-key lights retain their previous behavior.

If the projected light direction degenerates, the shader falls back to the
existing `N dot L` Toon band.

## Artist Workflow

1. Import the character and select its face material in Material Editor.
2. Set `Shading Model` to `Toon`.
3. Bind the linear Face Shadow texture in the `Face Shadow` slot.
4. Enable `Face Shadow` and set local `Face Forward` and `Face Right` for the
   asset. Defaults are `+Z` and `+X`.
5. Move the main light horizontally. Use `Mirror Face Shadow X` if the left and
   right response is reversed, then tune `Face Shadow Softness`.

## Performance And Limits

Only Face Shadow-enabled Toon materials with a bound texture perform the extra
texture lookup and angle calculation. Draw sorting, instancing, resource
ownership, and pass timing remain unchanged.

This minimum path does not include automatic face-material detection, a
texture-generation tool, a second UV set, animation-specific face frames,
multiple key lights, Shadow Ramp, or Hair Highlight. Visual correctness is
pending user acceptance on the target character and authored texture.
