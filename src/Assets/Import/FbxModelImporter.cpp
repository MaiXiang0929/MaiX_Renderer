// SPDX-License-Identifier: MIT
#include "Assets/Import/FbxModelImporter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "Assets/Import/ImageLoader.h"
#include "ufbx.h"

namespace AssetImport
{
namespace
{
std::string ToString(ufbx_string value)
{
    return value.data != nullptr
        ? std::string(value.data, value.length)
        : std::string();
}

bool ReadFileBytes(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes,
    std::string& error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        error = "Unable to open model file: " + path.u8string();
        return false;
    }

    const std::streamoff size = stream.tellg();
    if (size <= 0)
    {
        error = "Model file is empty: " + path.u8string();
        return false;
    }
    const auto unsignedSize = static_cast<std::uintmax_t>(size);
    if (unsignedSize > std::numeric_limits<std::size_t>::max() ||
        unsignedSize > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max()))
    {
        error = "Model file is too large to read: " + path.u8string();
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(unsignedSize));
    if (!stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())))
    {
        error = "Unable to read model file: " + path.u8string();
        return false;
    }
    return true;
}

cy::Vec3f ToVector(ufbx_vec3 vector)
{
    return {
        static_cast<float>(vector.x),
        static_cast<float>(vector.y),
        static_cast<float>(vector.z)
    };
}

void ExpandBounds(
    cy::Vec3f& minimum,
    cy::Vec3f& maximum,
    const cy::Vec3f& point,
    bool& initialized)
{
    if (!initialized)
    {
        minimum = point;
        maximum = point;
        initialized = true;
        return;
    }
    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
    maximum.z = std::max(maximum.z, point.z);
}

std::string Lowercase(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool IsSupportedTexture(const std::filesystem::path& path)
{
    const std::string extension = Lowercase(path.extension().string());
    return extension == ".png" || extension == ".jpg" ||
        extension == ".jpeg";
}

std::string RemoveBlenderDuplicateSuffix(std::string filename)
{
    const std::array<std::string, 3> extensions = {
        ".png", ".jpg", ".jpeg"
    };
    for (const std::string& extension : extensions)
    {
        const std::string marker = extension + ".";
        const std::size_t markerPosition = filename.rfind(marker);
        if (markerPosition == std::string::npos)
            continue;

        const std::size_t suffixPosition = markerPosition + marker.size();
        if (suffixPosition >= filename.size())
            continue;
        const bool numericSuffix = std::all_of(
            filename.begin() + static_cast<std::ptrdiff_t>(suffixPosition),
            filename.end(),
            [](unsigned char character) { return std::isdigit(character) != 0; });
        if (numericSuffix)
            return filename.substr(0, markerPosition + extension.size());
    }
    return filename;
}

void DiscoverTextureCandidates(ImportedModelData& model)
{
    const std::array<std::filesystem::path, 2> directories = {
        model.sourcePath.parent_path(),
        model.sourcePath.parent_path() / "textures"
    };
    for (const std::filesystem::path& directory : directories)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
            continue;
        for (const auto& entry : std::filesystem::directory_iterator(
                 directory, error))
        {
            if (error || !entry.is_regular_file() ||
                !IsSupportedTexture(entry.path()))
                continue;
            model.textureCandidates.push_back(entry.path());
        }
    }
    std::sort(
        model.textureCandidates.begin(), model.textureCandidates.end());
    model.textureCandidates.erase(std::unique(
        model.textureCandidates.begin(), model.textureCandidates.end()),
        model.textureCandidates.end());
}

std::filesystem::path ResolveTexturePath(
    ImportedModelData& model,
    const ufbx_texture* texture)
{
    if (texture == nullptr)
        return {};

    const std::array<ufbx_string, 3> names = {
        texture->relative_filename,
        texture->filename,
        texture->absolute_filename
    };
    for (ufbx_string name : names)
    {
        const std::string utf8Name = ToString(name);
        if (utf8Name.empty())
            continue;
        const std::filesystem::path supplied =
            std::filesystem::u8path(utf8Name);
        const std::array<std::filesystem::path, 3> attempts = {
            supplied,
            model.sourcePath.parent_path() / supplied,
            model.sourcePath.parent_path() / "textures" /
                supplied.filename()
        };
        for (const std::filesystem::path& attempt : attempts)
        {
            std::error_code error;
            if (std::filesystem::is_regular_file(attempt, error))
                return attempt.lexically_normal();
        }

        const std::string wanted = Lowercase(supplied.filename().u8string());
        auto match = std::find_if(
            model.textureCandidates.begin(), model.textureCandidates.end(),
            [&wanted](const std::filesystem::path& candidate)
            {
                return Lowercase(candidate.filename().u8string()) == wanted;
            });
        if (match != model.textureCandidates.end())
            return *match;

        const std::string fallbackWanted =
            RemoveBlenderDuplicateSuffix(wanted);
        if (fallbackWanted == wanted)
            continue;
        match = std::find_if(
            model.textureCandidates.begin(), model.textureCandidates.end(),
            [&fallbackWanted](const std::filesystem::path& candidate)
            {
                return Lowercase(candidate.filename().u8string()) ==
                    fallbackWanted;
            });
        if (match != model.textureCandidates.end())
        {
            model.warnings.push_back(
                "Texture reference '" + supplied.filename().u8string() +
                "' resolved to '" + match->filename().u8string() +
                "' after removing a duplicated numeric suffix.");
            return *match;
        }
    }
    return {};
}

const ufbx_texture* ResolveFileTexture(const ufbx_texture* texture)
{
    if (texture == nullptr)
        return nullptr;
    if (texture->type == UFBX_TEXTURE_FILE)
        return texture;
    return texture->file_textures.count > 0
        ? texture->file_textures.data[0]
        : nullptr;
}

std::uint32_t ImportTexture(
    ImportedModelData& model,
    const ufbx_texture* texture,
    std::unordered_map<const void*, std::uint32_t>& imported)
{
    const ufbx_texture* fileTexture = ResolveFileTexture(texture);
    if (fileTexture == nullptr)
        return InvalidImportedTextureIndex;
    const void* textureIdentity = fileTexture->video != nullptr
        ? static_cast<const void*>(fileTexture->video)
        : static_cast<const void*>(fileTexture);
    const auto existing = imported.find(textureIdentity);
    if (existing != imported.end())
        return existing->second;

    ImportedTextureData result;
    std::string sourceName = ToString(fileTexture->relative_filename);
    if (sourceName.empty())
        sourceName = ToString(fileTexture->filename);
    if (!sourceName.empty())
    {
        result.name = std::filesystem::u8path(sourceName)
            .filename().u8string();
    }
    if (result.name.empty())
        result.name = ToString(fileTexture->name);

    const ufbx_blob* content = nullptr;
    if (fileTexture->content.data != nullptr && fileTexture->content.size > 0)
        content = &fileTexture->content;
    else if (fileTexture->video != nullptr &&
        fileTexture->video->content.data != nullptr &&
        fileTexture->video->content.size > 0)
        content = &fileTexture->video->content;

    std::string imageError;
    if (content != nullptr)
    {
        result.embedded = true;
        if (!AssetImport::ImageLoader::LoadFromMemory(
                static_cast<const unsigned char*>(content->data),
                content->size,
                result.image,
                imageError))
        {
            model.warnings.push_back(
                "Embedded texture '" + result.name + "' was not decoded: " +
                imageError);
            return InvalidImportedTextureIndex;
        }
    }
    else
    {
        result.sourcePath = ResolveTexturePath(model, fileTexture);
        if (result.sourcePath.empty())
        {
            model.warnings.push_back(
                "Texture file was not found for material input '" +
                result.name + "'.");
            return InvalidImportedTextureIndex;
        }
        if (!AssetImport::ImageLoader::LoadFromFile(
                result.sourcePath, result.image, imageError))
        {
            model.warnings.push_back(
                "Texture '" + result.sourcePath.u8string() +
                "' was not decoded: " + imageError);
            return InvalidImportedTextureIndex;
        }
    }

    const std::uint32_t index =
        static_cast<std::uint32_t>(model.textures.size());
    model.textures.push_back(std::move(result));
    imported.emplace(textureIdentity, index);
    return index;
}

void GenerateFallbackTangents(std::vector<Vertex>& vertices)
{
    for (std::size_t index = 0; index + 2 < vertices.size(); index += 3)
    {
        Vertex* triangle = vertices.data() + index;
        const cy::Vec3f edge1 =
            triangle[1].Position - triangle[0].Position;
        const cy::Vec3f edge2 =
            triangle[2].Position - triangle[0].Position;
        const cy::Vec2f uv1 =
            triangle[1].TexCoord - triangle[0].TexCoord;
        const cy::Vec2f uv2 =
            triangle[2].TexCoord - triangle[0].TexCoord;
        const float determinant = uv1.x * uv2.y - uv1.y * uv2.x;

        cy::Vec3f tangent(1.0f, 0.0f, 0.0f);
        cy::Vec3f bitangent(0.0f, 1.0f, 0.0f);
        if (std::abs(determinant) > 1.0e-8f)
        {
            const float inverse = 1.0f / determinant;
            tangent = (edge1 * uv2.y - edge2 * uv1.y) * inverse;
            bitangent = (edge2 * uv1.x - edge1 * uv2.x) * inverse;
        }
        if (tangent.Length() > 1.0e-8f)
            tangent.Normalize();

        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        {
            const cy::Vec3f normal = triangle[vertexIndex].Normal;
            cy::Vec3f orthogonalTangent =
                tangent - normal * normal.Dot(tangent);
            if (orthogonalTangent.Length() <= 1.0e-8f)
            {
                orthogonalTangent = std::abs(normal.y) < 0.999f
                    ? cy::Vec3f(normal.z, 0.0f, -normal.x)
                    : cy::Vec3f(1.0f, 0.0f, 0.0f);
            }
            orthogonalTangent.Normalize();
            const cy::Vec3f cross(
                normal.y * orthogonalTangent.z -
                    normal.z * orthogonalTangent.y,
                normal.z * orthogonalTangent.x -
                    normal.x * orthogonalTangent.z,
                normal.x * orthogonalTangent.y -
                    normal.y * orthogonalTangent.x);
            const float sign = cross.Dot(bitangent) < 0.0f ? -1.0f : 1.0f;
            triangle[vertexIndex].Tangent = {
                orthogonalTangent.x,
                orthogonalTangent.y,
                orthogonalTangent.z,
                sign };
        }
    }
}

bool BuildMeshPart(
    const ufbx_node& node,
    const ufbx_mesh_part& part,
    std::uint32_t materialIndex,
    bool flipUv,
    ImportedMeshData& result,
    std::string& warning)
{
    const ufbx_mesh& mesh = *node.mesh;
    std::vector<Vertex> flatVertices;
    flatVertices.reserve(part.num_triangles * 3);
    std::vector<std::uint32_t> triangleIndices(
        std::max<std::size_t>(mesh.max_face_triangles * 3, 3));

    const ufbx_vertex_vec3& positions = mesh.skinned_position.exists
        ? mesh.skinned_position
        : mesh.vertex_position;
    const ufbx_vertex_vec3& normals = mesh.skinned_normal.exists
        ? mesh.skinned_normal
        : mesh.vertex_normal;
    const bool positionsAreLocal = mesh.skinned_is_local;
    const ufbx_matrix normalToWorld =
        ufbx_matrix_for_normals(&node.geometry_to_world);
    for (std::size_t partFaceIndex = 0;
        partFaceIndex < part.face_indices.count;
        ++partFaceIndex)
    {
        const std::uint32_t faceIndex = part.face_indices.data[partFaceIndex];
        if (faceIndex >= mesh.faces.count ||
            (mesh.face_hole.count > faceIndex &&
                mesh.face_hole.data[faceIndex]))
            continue;
        const ufbx_face face = mesh.faces.data[faceIndex];
        if (face.num_indices < 3)
            continue;
        const std::uint32_t triangleCount = ufbx_triangulate_face(
            triangleIndices.data(), triangleIndices.size(), &mesh, face);
        for (std::size_t triangleVertex = 0;
            triangleVertex < static_cast<std::size_t>(triangleCount) * 3;
            ++triangleVertex)
        {
            const std::uint32_t meshIndex = triangleIndices[triangleVertex];
            Vertex vertex{};
            ufbx_vec3 position = ufbx_get_vertex_vec3(
                &positions, meshIndex);
            if (positionsAreLocal)
            {
                position = ufbx_transform_position(
                    &node.geometry_to_world, position);
            }
            vertex.Position = ToVector(position);

            ufbx_vec3 normal = normals.exists
                ? ufbx_get_vertex_vec3(&normals, meshIndex)
                : ufbx_vec3{ 0.0, 1.0, 0.0 };
            if (positionsAreLocal)
                normal = ufbx_transform_direction(&normalToWorld, normal);
            vertex.Normal = normals.exists
                ? ToVector(normal)
                : cy::Vec3f(0.0f, 1.0f, 0.0f);
            if (vertex.Normal.Length() > 1.0e-8f)
                vertex.Normal.Normalize();
            if (mesh.vertex_uv.exists)
            {
                const ufbx_vec2 uv = ufbx_get_vertex_vec2(
                    &mesh.vertex_uv, meshIndex);
                vertex.TexCoord = {
                    static_cast<float>(uv.x),
                    flipUv ? 1.0f - static_cast<float>(uv.y)
                           : static_cast<float>(uv.y)
                };
            }
            flatVertices.push_back(vertex);
        }
    }

    if (flatVertices.empty())
        return false;
    // Runtime skinning is intentionally out of scope. Rebuild tangents from
    // the baked static pose so normal mapping follows the deformed geometry.
    GenerateFallbackTangents(flatVertices);

    result.indices.resize(flatVertices.size());
    ufbx_vertex_stream stream{};
    stream.data = flatVertices.data();
    stream.vertex_count = flatVertices.size();
    stream.vertex_size = sizeof(Vertex);
    ufbx_error indexError{};
    const std::size_t uniqueVertexCount = ufbx_generate_indices(
        &stream,
        1,
        result.indices.data(),
        result.indices.size(),
        nullptr,
        &indexError);
    if (uniqueVertexCount == 0)
    {
        for (std::size_t index = 0; index < result.indices.size(); ++index)
            result.indices[index] = static_cast<std::uint32_t>(index);
        warning = "Vertex deduplication failed for '" + ToString(node.name) +
            "'; using a flat triangle list.";
    }
    else
    {
        flatVertices.resize(uniqueVertexCount);
    }

    result.name = ToString(node.name);
    result.materialIndex = materialIndex;
    result.localToWorld = cy::Matrix4f::Identity();
    result.vertices = std::move(flatVertices);

    cy::Vec3f minimum;
    cy::Vec3f maximum;
    bool initialized = false;
    for (const Vertex& vertex : result.vertices)
        ExpandBounds(minimum, maximum, vertex.Position, initialized);
    result.boundsCenter = (minimum + maximum) * 0.5f;
    result.boundsRadius = 0.0f;
    for (const Vertex& vertex : result.vertices)
    {
        result.boundsRadius = std::max(
            result.boundsRadius,
            (vertex.Position - result.boundsCenter).Length());
    }
    return true;
}

float Median(std::vector<float> values)
{
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    return values[middle];
}

void AppendDistantSectionWarning(ImportedModelData& model)
{
    if (model.meshes.size() < 4)
        return;

    std::vector<float> centerX;
    std::vector<float> centerY;
    std::vector<float> centerZ;
    centerX.reserve(model.meshes.size());
    centerY.reserve(model.meshes.size());
    centerZ.reserve(model.meshes.size());
    for (const ImportedMeshData& mesh : model.meshes)
    {
        centerX.push_back(mesh.boundsCenter.x);
        centerY.push_back(mesh.boundsCenter.y);
        centerZ.push_back(mesh.boundsCenter.z);
    }
    const cy::Vec3f medianCenter(
        Median(std::move(centerX)),
        Median(std::move(centerY)),
        Median(std::move(centerZ)));

    std::vector<float> distances;
    distances.reserve(model.meshes.size());
    for (const ImportedMeshData& mesh : model.meshes)
        distances.push_back((mesh.boundsCenter - medianCenter).Length());

    const float typicalDistance = Median(distances);
    const float warningDistance = std::max(10.0f, typicalDistance * 10.0f);
    std::vector<std::string> distantSections;
    for (std::size_t index = 0; index < model.meshes.size(); ++index)
    {
        if (distances[index] > warningDistance)
            distantSections.push_back(model.meshes[index].name);
    }
    if (distantSections.empty())
        return;

    std::ostringstream message;
    message << "Detected " << distantSections.size()
            << " distant mesh sections more than " << warningDistance
            << " meters from the median section center; total bounds may be "
               "unsuitable for automatic framing: ";
    const std::size_t displayedCount =
        std::min<std::size_t>(distantSections.size(), 4);
    for (std::size_t index = 0; index < displayedCount; ++index)
    {
        if (index > 0)
            message << ", ";
        message << distantSections[index];
    }
    if (displayedCount < distantSections.size())
        message << ", ...";
    model.warnings.push_back(message.str());
}
}

ModelImportResult FbxModelImporter::Import(
    const std::filesystem::path& path,
    const FbxImportOptions& options)
{
    ModelImportResult result;
    result.model.sourcePath = path;
    result.model.name = path.stem().u8string();

    std::vector<std::uint8_t> fileBytes;
    if (!ReadFileBytes(path, fileBytes, result.error))
        return result;

    ufbx_load_opts loadOptions{};
    loadOptions.file_format = UFBX_FILE_FORMAT_FBX;
    loadOptions.generate_missing_normals = options.generateMissingNormals;
    loadOptions.evaluate_skinning = true;
    loadOptions.clean_skin_weights = true;
    loadOptions.normalize_normals = true;
    loadOptions.normalize_tangents = true;
    loadOptions.use_blender_pbr_material = true;
    loadOptions.ignore_missing_external_files = true;
    loadOptions.load_external_files = false;
    loadOptions.target_axes = ufbx_axes_right_handed_y_up;
    loadOptions.target_unit_meters = 1.0;
    loadOptions.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    loadOptions.geometry_transform_handling =
        UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE;
    loadOptions.node_depth_limit = 512;

    const std::string sourcePathUtf8 = path.u8string();
    loadOptions.filename.data = sourcePathUtf8.data();
    loadOptions.filename.length = sourcePathUtf8.size();

    ufbx_error loadError{};
    ufbx_scene* scene = ufbx_load_memory(
        fileBytes.data(), fileBytes.size(), &loadOptions, &loadError);
    if (scene == nullptr)
    {
        std::array<char, 1024> message{};
        ufbx_format_error(message.data(), message.size(), &loadError);
        result.error = "FBX parse failed: " + std::string(message.data());
        return result;
    }
    const std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> sceneOwner(
        scene, &ufbx_free_scene);

    result.model.sourceMeshCount = scene->meshes.count;
    result.model.sourceMaterialCount = scene->materials.count;
    result.model.skinDeformerCount = scene->skin_deformers.count;
    result.model.animationStackCount = scene->anim_stacks.count;
    DiscoverTextureCandidates(result.model);

    std::unordered_map<const ufbx_material*, std::uint32_t> materialIndices;
    std::unordered_map<const void*, std::uint32_t> textureIndices;
    result.model.materials.reserve(scene->materials.count + 1);
    for (const ufbx_material* material : scene->materials)
    {
        ImportedMaterialData importedMaterial;
        importedMaterial.name = ToString(material->name);
        if (importedMaterial.name.empty())
            importedMaterial.name = "Material " +
                std::to_string(result.model.materials.size());

        const ufbx_material_map& baseColor = material->pbr.base_color;
        if (baseColor.has_value)
        {
            importedMaterial.baseColor = {
                static_cast<float>(baseColor.value_vec4.x),
                static_cast<float>(baseColor.value_vec4.y),
                static_cast<float>(baseColor.value_vec4.z)
            };
        }
        else if (material->fbx.diffuse_color.has_value)
        {
            const ufbx_vec3 color =
                material->fbx.diffuse_color.value_vec3;
            importedMaterial.baseColor = ToVector(color);
        }
        if (material->pbr.roughness.has_value)
            importedMaterial.roughness = std::clamp(
                static_cast<float>(material->pbr.roughness.value_real),
                0.045f, 1.0f);
        if (material->pbr.metalness.has_value)
            importedMaterial.metallic = std::clamp(
                static_cast<float>(material->pbr.metalness.value_real),
                0.0f, 1.0f);
        if (material->pbr.opacity.has_value)
            importedMaterial.opacity = std::clamp(
                static_cast<float>(material->pbr.opacity.value_real),
                0.0f, 1.0f);

        const ufbx_texture* baseTexture = baseColor.texture != nullptr
            ? baseColor.texture
            : material->fbx.diffuse_color.texture;
        importedMaterial.baseColorTexture = ImportTexture(
            result.model, baseTexture, textureIndices);

        const std::uint32_t materialIndex =
            static_cast<std::uint32_t>(result.model.materials.size());
        result.model.materials.push_back(std::move(importedMaterial));
        materialIndices.emplace(material, materialIndex);
    }
    std::uint32_t defaultMaterialIndex =
        std::numeric_limits<std::uint32_t>::max();
    const auto ensureDefaultMaterial = [&]()
    {
        if (defaultMaterialIndex !=
            std::numeric_limits<std::uint32_t>::max())
        {
            return defaultMaterialIndex;
        }

        ImportedMaterialData defaultMaterial;
        defaultMaterial.name = "Default Material";
        defaultMaterialIndex = static_cast<std::uint32_t>(
            result.model.materials.size());
        result.model.materials.push_back(std::move(defaultMaterial));
        return defaultMaterialIndex;
    };
    if (result.model.materials.empty())
        ensureDefaultMaterial();

    bool modelBoundsInitialized = false;
    for (const ufbx_node* node : scene->nodes)
    {
        if (node == nullptr || node->mesh == nullptr || !node->visible)
            continue;
        const ufbx_mesh& mesh = *node->mesh;
        for (std::size_t partIndex = 0;
            partIndex < mesh.material_parts.count;
            ++partIndex)
        {
            const ufbx_material* material = partIndex < node->materials.count
                ? node->materials.data[partIndex]
                : nullptr;
            std::uint32_t materialIndex = defaultMaterialIndex;
            const auto materialMatch = materialIndices.find(material);
            if (materialMatch != materialIndices.end())
                materialIndex = materialMatch->second;
            else
                materialIndex = ensureDefaultMaterial();

            ImportedMeshData importedMesh;
            std::string warning;
            if (!BuildMeshPart(
                    *node,
                    mesh.material_parts.data[partIndex],
                    materialIndex,
                    options.flipUv,
                    importedMesh,
                    warning))
                continue;
            if (!warning.empty())
                result.model.warnings.push_back(std::move(warning));
            if (materialIndex < result.model.materials.size())
            {
                importedMesh.name += ":" +
                    result.model.materials[materialIndex].name;
            }
            for (const Vertex& vertex : importedMesh.vertices)
            {
                ExpandBounds(
                    result.model.boundsMin,
                    result.model.boundsMax,
                    vertex.Position,
                    modelBoundsInitialized);
            }
            result.model.meshes.push_back(std::move(importedMesh));
        }
    }

    if (result.model.skinDeformerCount > 0)
    {
        result.model.warnings.push_back(
            "The evaluated source pose was baked as static geometry; "
            "runtime skinning data was not retained.");
    }
    AppendDistantSectionWarning(result.model);
    if (result.model.meshes.empty())
        result.error = "The FBX contains no renderable triangle sections.";

    return result;
}
}
