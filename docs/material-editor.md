# Minimal Material Editor

The ImGui Material Editor is a thin editor-facing boundary over Renderer-owned
materials. It enumerates stable `MaterialHandle` values, requests a
`Renderer::MaterialSnapshot`, and submits a complete `MaterialProperties`
value through `Renderer::UpdateMaterial`.

The editor never stores a `Material*` and cannot mutate the resource table.
Renderer validates the handle, clamps artist-facing scalar ranges, updates the
material used by Forward/Reflection/Translucency, and propagates Blend Mode to
all matching scene proxies so queue classification remains correct.

The GPU path is unchanged: on the next draw, each pass binds the updated
scalar uniforms and existing texture slots. Texture replacement, material
instances, serialization, and undo/redo remain future milestones.

Run `OpenGL_Project.exe --material-lab` to inspect the four PBR materials and
change Base Color, Metallic, Roughness, AO, Normal Scale, Opacity, or Blend
Mode at runtime.
