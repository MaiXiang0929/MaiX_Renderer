# PBR Material Texture Workflow

## Material contract

MaiX Renderer uses a metallic-roughness material model. Scalar properties are
artist-facing factors and texture channels multiply those factors.

| Semantic | Texture unit offset | Color space | Channel contract |
| --- | ---: | --- | --- |
| Base Color | 0 | sRGB | RGBA |
| ORM | 1 | Linear | R=AO, G=Roughness, B=Metallic |
| Normal | 2 | Linear | Tangent-space XYZ |
| Displacement | 3 | Linear | R=height |
| Legacy Specular | 1 | Linear | RGB tint, only when ORM is absent |

ORM and Legacy Specular deliberately share unit 1. ORM takes precedence when
both are assigned. This preserves old OBJ/MTL materials without expanding the
four material texture units used by Forward rendering.

`Material::SetTexture()` rejects a texture whose stored color-space metadata
does not match its semantic. Convenience setters route through the same check.
The Renderer owns each Material; the Material owns shared references to its
textures, while RenderItem and scene proxies remain non-owning.

## Factor and fallback rules

The fragment shader resolves inputs as follows:

```text
linearBaseColor = Linear(baseColorFactor) * sampledBaseColor.rgb
ambientOcclusion = clamp(aoFactor * sampledOrm.r, 0, 1)
roughness = clamp(roughnessFactor * sampledOrm.g, 0.045, 1)
metallic = clamp(metallicFactor * sampledOrm.b, 0, 1)
```

Missing Base Color and ORM textures return an implicit factor of 1. Base Color
textures use `GL_SRGB8_ALPHA8`, so OpenGL performs their RGB decode during
sampling. The Base Color constant is treated as an sRGB UI value and converted
once in the shader. Alpha is never gamma converted.

## Normal coordinate spaces

CPU mesh import produces a local-space tangent and handedness per vertex. The
vertex or tessellation-evaluation shader transforms normal and tangent into
view space. The fragment shader then performs:

```text
sampled tangent-space normal
    -> scale tangent X/Y by normalScale
    -> normalize
    -> multiply by view-space TBN
    -> view-space shading normal
```

Lighting positions, view direction, light vectors, and the resolved normal are
therefore all evaluated in view space. Standard, instanced, and tessellated
geometry share the same PBR fragment shader.

## Material Lab

Run the opt-in validation scene with:

```text
OpenGL_Project.exe --material-lab
```

It creates four materials on one shared teapot Mesh in a 2x2 layout:

- copper with a low-roughness ORM checker;
- blue dielectric plastic using scalar factors only;
- ceramic using scalar factors plus the bundled Normal Map;
- rough metal with a high-roughness ORM checker.

The mode is mutually exclusive with `--instance-grid` because they validate
different ownership and batching cases. The final RenderDoc capture is
`captures/maix_material_lab_frame2.rdc`; CLI thumbnail replay confirms four
distinct materials, reflection output, four material batches, and one shared
Mesh.

## Cost and limitations

An ORM material adds one packed texture sample instead of three independent
samples. A Normal Map adds one sample plus tangent-basis normalization. Missing
maps do not sample textures. Displacement remains tessellation-only.

The current workflow does not provide texture transforms, sampler objects,
material instances, IBL prefiltering, clearcoat, transmission, or a full glTF
importer. Material editing is still performed through code; an ImGui Material
Editor is a separate tool milestone.
