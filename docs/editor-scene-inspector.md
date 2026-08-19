# Scene Window and Inspector

The editor now exposes the CPU-owned scene state through two ImGui panels:

- `Scene` lists the active model and all editable lights.
- `Inspector` edits the selected model transform or light parameters.

Both panels share `EditorSelection` with viewport picking and Gizmo editing.
They do not own Mesh, Material, or OpenGL resources. Model changes call
`ApplyEditableModelTransform`, while light changes update the corresponding
`LightSceneProxy` through `Renderer::UpdateLight`.

The GPU path is unchanged. On the next frame, the updated primitive matrices
and light values are consumed by the existing Shadow, Reflection, Forward,
Outline, Translucency, and EditorPrimitive passes.

Run the application with `--material-lab` for a compact editor demonstration.
Select an object in the Scene panel or viewport, edit values in Inspector, and
verify that the lighting, shadow, reflection, and outline follow the change.

The current hierarchy is intentionally flat: one active model plus the
editable lights. Per-primitive hierarchy, scene serialization, object
creation/deletion, and undo/redo remain future editor work.
