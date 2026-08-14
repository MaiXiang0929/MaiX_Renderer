# Shared-Resource Instance Benchmark

## Launch configuration

The benchmark is disabled by default. It can be enabled without changing the
normal material-map positional arguments:

```text
OpenGL_Project.exe --instance-grid 16
OpenGL_Project.exe normal.png displacement.png --instance-grid 8
```

`N` is limited to `1..32`. The default scene still submits one opaque teapot.

## CPU ownership and layout

`ApplicationOptions` parses the launch configuration before OpenGL starts.
`InstanceGrid` produces a centered X/Y grid and a conservative scene radius.
Application uploads one teapot `Mesh` and one `Material`, then submits one
`PrimitiveSceneProxy` per transform with the same handles.

```text
CLI grid size
    -> centered CPU transforms
    -> one shared MeshHandle + MaterialHandle
    -> N * N PrimitiveSceneProxy objects
    -> per-view culling and RenderItem sorting
    -> contiguous Shader/Material/Mesh batches
    -> one instanced draw per batch (up to 256 instances per draw)
```

The benchmark scene radius drives camera distance, the camera far plane, ground
extent, and the shadow projection. This keeps large grids measurable without
disabling the production culling path.

## Baseline results

Environment: OpenGL 4.0, NVIDIA GeForce RTX 4060 Ti, 1920x1080 Debug build.
Each row is a separate five-second run. GPU values are the final EMA snapshot,
so they are directional rather than a statistically rigorous performance test.

| Grid | Opaque instances | Mesh resources | Material resources | Shadow draws | Reflection draws | Forward draws | GPU frame EMA |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1x1 | 1 | 2 | 4 | 1 | 5 | 3 | 0.747 ms |
| 8x8 | 64 | 2 | 4 | 64 | 25 | 66 | 0.942 ms |
| 16x16 | 256 | 2 | 4 | 256 | 65 | 258 | 1.386 ms |

The resource counts include the separate translucent test mesh and its three
materials. They remain constant at every grid size, proving that the opaque
instances share their GPU resources. Reflection draw counts are lower because
that view performs independent frustum culling.

At 16x16, Forward records:

```text
material requests/changes = 256/1
mesh requests/changes     = 258/3
texture requests/changes  = 1030/9
```

The stable resource ordering is effective, but the renderer still submits one
draw and repeats material uniform/texture work for every opaque instance.

## RenderDoc capture

The local capture is `captures/maix_instances_16_frame146.rdc` (about 95.9 MiB).
It is excluded from source control. `renderdoccmd thumb` successfully replayed
the capture, and the thumbnail confirms all 259 scene primitives visible in the
main view plus the independently culled reflection result.

## Instanced implementation

After stable resource sorting, `BuildOpaqueRenderBatches()` groups contiguous
items by Shader, Material, and Mesh IDs. Forward, Reflection, and Shadow use the
batch list for standard triangle rendering. Single-item batches keep the
existing non-instanced path; translucent and tessellated rendering are also
unchanged.

The CPU computes one model-view matrix per visible instance. `ForwardPass` and
`ShadowPass` each own a six-region streaming `InstanceBuffer`; Reflection reuses
the Forward resource. Each upload region prevents the CPU from immediately
overwriting data that an earlier frame may still consume. The OpenGL 4.0
minimum 16 KiB uniform-block size holds 256 `mat4` values, so a larger batch is
split only at that portable limit.

```text
CPU local-to-world
    -> view * local-to-world = model-view
    -> one aligned std140 upload per view
    -> bind one 16 KiB range per batch chunk
GPU model-view * local position = view position
    -> projection-from-view * view position = clip position
    -> light-from-view * view position = shadow position
```

The standard and instanced PBR shaders therefore agree on coordinate spaces.
The instanced shader deliberately computes the normal matrix from model-view,
matching the original shader and keeping each instance record at 64 bytes.

## Optimized submission results

Environment is unchanged. The optimized runs were collected independently, so
the final GPU EMA remains directional. Reflection showed periodic timing spikes
during both optimized and non-instanced A/B runs.

| Grid | Opaque instances | Shadow draws / instanced | Reflection draws / instanced | Forward draws / instanced | GPU frame EMA |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1x1 | 1 | 1 / 0 | 5 / 0 | 3 / 0 | 0.780 ms |
| 8x8 | 64 | 1 / 1 | 2 / 1 | 3 / 1 | 1.323 ms |
| 16x16 | 256 | 1 / 1 | 2 / 1 | 3 / 1 | 1.776 ms |

The 16x16 Forward path dropped from 258 Draws to 3, including skybox and
reflection ground. Its opaque material requests dropped from 256 to 1 and
texture requests from 1030 to 10. Shadow dropped from 256 Draws to 1, while
Reflection dropped from 65 to 2 for the captured view.

A same-build 16x16 A/B check measured steady frame samples around
`1.47-1.49 ms` with instancing and `1.50-1.52 ms` with per-item Forward and
Reflection submission. This is not a statistically rigorous GPU benchmark,
but it confirms that the final compact layout does not reproduce the clear GPU
regression seen in discarded 240-byte UBO and vertex-attribute prototypes. The
primary verified gain is CPU/API submission reduction, not a claimed GPU
speedup.

The optimized local RenderDoc capture is
`captures/maix_instanced_16_frame2.rdc` (about 95.8 MiB). CLI thumbnail replay
succeeded and confirmed the grid, reflection result, and the `1/2/3` Draw
counts for Shadow/Reflection/Forward. Capture files and thumbnails remain
excluded from source control.

Tessellation intentionally falls back to per-item patch submission because its
per-material displacement path has not been made instanced. An 8x8 runtime
toggle verified 64 Shadow, 25 Reflection, and 66 Forward non-instanced Draws.

A standalone texture cache remains deferred: instancing already removes the
dominant repeated material and texture requests in the measured shared-resource
case. Re-evaluate it only with a scene containing many distinct batches.
