# Editor Light Visualization

## Pipeline position

```text
Shadow -> Reflection -> Forward -> Translucency -> Bloom -> PostProcess
                                                        |          |
                                          display color |          | editor overlay
                                                        v          v
                                                         Present -> ImGui
```

`EditorPrimitivePass` executes after `PostProcessPass` and renders into its own
full-resolution `RGBA8` Overlay Buffer. `PresentPass` composites the display
color and overlay. Editor visualization therefore never enters HDR Scene Color,
Bloom, exposure, or tone mapping.

## Resource ownership

`EditorPrimitivePass` owns:

- A full-resolution `RGBA8` color target.
- No depth/stencil attachment.
- A mip chain for transformed or minified Present sampling.
- Billboard and line shaders, VAOs, and VBOs.

Resize follows the window framebuffer. The target is cleared to transparent
black every frame, including when editor primitives are disabled, so stale
gizmos cannot survive an enable-state change.

## Data flow

```text
Application updates LightSceneProxy
    -> RenderScene::BuildRenderView
    -> RenderView::lights
    -> EditorPrimitivePass
    -> premultiplied editorOverlayTexture
    -> PresentPass composite
```

The CPU owns the overlay lifetime and submits light proxy parameters. The GPU
expands billboards, transforms spot cones, rasterizes them into the overlay,
and composites the overlay with display-ready scene color.

## Alpha and color space

Editor shaders output premultiplied alpha and use:

```text
source      = ONE
destination = ONE_MINUS_SRC_ALPHA
```

Present evaluates `overlay.rgb + scene.rgb * (1 - overlay.a)`. Both inputs are
display-encoded `RGBA8` values, so editor colors remain stable when Bloom,
exposure, or tone mapping changes. Premultiplication also keeps filtered
billboard edges free of dark fringes.

## Current depth behavior

Depth testing and depth writes remain disabled. Light gizmos are always visible,
matching the existing editor behavior. Depth-aware, X-ray, picking, and hit
proxy paths remain future work.
