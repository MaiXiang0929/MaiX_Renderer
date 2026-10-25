// SPDX-License-Identifier: MIT
#include "Engine/Asset/Importer/FbxModelImporter.h"

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

#include "Engine/Asset/Importer/ImageLoader.h"
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
                return std::filesystem::absolute(attempt).lexically_normal();
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

const ufbx_texture* SupportedTexture(ImportedModelData& model,const ufbx_scene& scene,
    const ufbx_material& material,const ufbx_material_map& map,const char* input)
{
    const ufbx_texture* texture=map.texture;
    if(!texture) return nullptr;
    std::string reason;
    if(!map.texture_enabled || map.feature_disabled) reason="the material disables this texture input";
    else if(texture->type!=UFBX_TEXTURE_FILE || texture->shader || texture->layers.count)
        reason="layered/procedural/shader texture graphs are unsupported";
    else if(texture->has_uv_transform) reason="nonidentity texture UV transforms are unsupported";
    else if(texture->wrap_u!=UFBX_WRAP_REPEAT || texture->wrap_v!=UFBX_WRAP_REPEAT)
        reason="texture wrap modes other than repeat are unsupported";
    else
    {
        // ufbx exposes one mapped texture; inspect original connections so a
        // duplicate property connection cannot be silently reduced to the first.
        for(const ufbx_connection& selected:material.element.connections_dst)
        {
            if(selected.src!=&texture->element) continue;
            unsigned count=0;
            for(const ufbx_connection& connection:material.element.connections_dst)
                if(connection.src->type==UFBX_ELEMENT_TEXTURE &&
                    ToString(connection.dst_prop)==ToString(selected.dst_prop)) ++count;
            if(count>1) { reason="multiple textures connected to one property are unsupported"; break; }
        }
        for(const ufbx_node* node:scene.nodes)
        {
            if(!reason.empty()) break;
            if(!node->mesh || std::find(node->materials.begin(),node->materials.end(),&material)==node->materials.end()) continue;
            const auto& uvSets=node->mesh->uv_sets;
            if(uvSets.count!=1 || !node->mesh->vertex_uv.exists)
                reason="materials require exactly one usable mesh UV set";
            else if(texture->uv_set.length && ToString(texture->uv_set)!=ToString(uvSets.data[0].name))
                reason="the texture selects a different mesh UV set";
        }
    }
    if(reason.empty()) return texture;
    model.warnings.push_back("Material '"+ToString(material.name)+"' "+input+" texture ignored: "+reason+".");
    return nullptr;
}

std::uint32_t ImportTexture(
    ImportedModelData& model,
    const ufbx_texture* texture,
    std::unordered_map<const void*, std::uint32_t>& imported)
{
    const ufbx_texture* fileTexture = texture;
    if (fileTexture == nullptr)
        return InvalidImportedTextureIndex;
    const void* textureIdentity = fileTexture->video != nullptr
        ? static_cast<const void*>(fileTexture->video)
        : static_cast<const void*>(fileTexture);
    const auto existing = imported.find(textureIdentity);
    if (existing != imported.end())
        return existing->second;

    ImportedTextureData result;
    result.sourceId=static_cast<std::uint64_t>(fileTexture->video ?
        fileTexture->video->element_id : fileTexture->element_id)+1;
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

    TextureSource source;
    source.image.path=result.embedded ? model.sourcePath : result.sourcePath;
    source.image.embeddedId=result.embedded ? result.sourceId : 0;
    result.reconstruction=std::move(source);
    const std::uint32_t index =
        static_cast<std::uint32_t>(model.textures.size());
    model.textures.push_back(std::move(result));
    imported.emplace(textureIdentity, index);
    return index;
}

std::uint32_t AppendDerivedTexture(ImportedModelData& model,const TextureSource& source,const std::string& name)
{
    ImportedTextureData derived;
    derived.name=name;
    std::string error;
    const auto loader=[&](const TextureSourceImage& input,ImageData& image,std::string& loadError)
    {
        for(const auto& raw:model.textures)
            if(raw.reconstruction && raw.reconstruction->kind==TextureSourceKind::Image &&
                raw.reconstruction->image.path==input.path && raw.reconstruction->image.embeddedId==input.embeddedId)
            { image=raw.image; return true; }
        loadError="Raw imported texture is unavailable."; return false;
    };
    if(!BuildTextureSourceImage(source,loader,derived.image,error))
    { model.warnings.push_back("Derived texture '"+name+"' ignored: "+error); return InvalidImportedTextureIndex; }
    derived.reconstruction=source;
    const auto index=static_cast<std::uint32_t>(model.textures.size());
    model.textures.push_back(std::move(derived));
    return index;
}

void ImportPbrTextures(ImportedModelData& model,const ufbx_scene& scene,const ufbx_material& material,
    const FbxImportOptions& options,ImportedMaterialData& result,std::unordered_map<const void*,std::uint32_t>& imported)
{
    const auto& normal=material.pbr.normal_map.texture ? material.pbr.normal_map : material.fbx.normal_map;
    const auto normalIndex=ImportTexture(model,SupportedTexture(model,scene,material,normal,"normal"),imported);
    if(normalIndex!=InvalidImportedTextureIndex)
    {
        TextureSource source; source.kind=TextureSourceKind::Normal;
        source.image=model.textures[normalIndex].reconstruction->image; source.flipGreen=options.flipNormalGreen;
        result.normalTexture=AppendDerivedTexture(model,source,result.name+":Normal");
    }
    if(material.fbx.bump.texture)
        model.warnings.push_back("Material '"+result.name+"' bump texture is unsupported; no height-to-normal conversion is performed.");
    const auto& glossiness = material.pbr.glossiness;
    if (glossiness.texture && glossiness.texture_enabled && !glossiness.feature_disabled)
        model.warnings.push_back("Material '" + result.name + "' glossiness texture is unsupported; no glossiness-to-roughness conversion is performed.");
    const std::array<const ufbx_material_map*,3> maps={&material.pbr.ambient_occlusion,&material.pbr.roughness,&material.pbr.metalness};
    const std::array<const char*,3> names={"ambient occlusion","roughness","metallic"};
    TextureSource packed; packed.kind=TextureSourceKind::Orm;
    std::array<bool,3> accepted{};
    for(unsigned channel=0;channel<3;++channel)
    {
        const auto& map=*maps[channel];
        const auto index=ImportTexture(model,SupportedTexture(model,scene,material,map,names[channel]),imported);
        if(index==InvalidImportedTextureIndex) continue;
        const auto& raw=model.textures[index];
        bool grayscale=true;
        for(std::size_t i=0;i<raw.image.pixels.size();i+=4)
            if(raw.image.pixels[i]!=raw.image.pixels[i+1] || raw.image.pixels[i]!=raw.image.pixels[i+2]) { grayscale=false; break; }
        if(!grayscale)
        { model.warnings.push_back("Material '"+result.name+"' "+names[channel]+" texture ignored: colored scalar image has ambiguous channel semantics."); continue; }
        packed.ormInputs[channel]=TextureChannelInput{raw.reconstruction->image,0};
        packed.width=std::max(packed.width,raw.image.width); packed.height=std::max(packed.height,raw.image.height);
        accepted[channel]=true;
    }
    if(packed.width && packed.height)
    {
        result.ormTexture=AppendDerivedTexture(model,packed,result.name+":ORM");
        // A connected map without an authored factor is multiplied by one, not
        // the no-map metallic zero or roughness fallback. Explicit zero remains zero.
        if(result.ormTexture!=InvalidImportedTextureIndex)
        {
            if(accepted[0] && !maps[0]->has_value) result.ambientOcclusion=1.0f;
            if(accepted[1] && !maps[1]->has_value) result.roughness=1.0f;
            if(accepted[2] && !maps[2]->has_value) result.metallic=1.0f;
        }
    }
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
            if (!std::isfinite(vertex.Position.x) || !std::isfinite(vertex.Position.y) ||
                !std::isfinite(vertex.Position.z))
            {
                warning = "Non-finite converted vertex position in '" + ToString(node.name) + "'.";
                return false;
            }

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
                    static_cast<float>(uv.y)
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
    // V is flipped only for top-down image addressing. Build the baked-pose
    // tangent frame from authored UVs so a +Y normal map keeps its authored B.
    if (flipUv && mesh.vertex_uv.exists)
        for (auto& vertex : flatVertices)
            vertex.TexCoord.y = 1.0f - vertex.TexCoord.y;

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
    if (!std::isfinite(result.boundsCenter.x) || !std::isfinite(result.boundsCenter.y) ||
        !std::isfinite(result.boundsCenter.z) || !std::isfinite(result.boundsRadius))
    {
        warning = "Invalid converted bounds in '" + ToString(node.name) + "'.";
        return false;
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
    std::error_code pathError;
    result.model.sourcePath = std::filesystem::absolute(path,pathError).lexically_normal();
    if(pathError) { result.error="Unable to resolve model path: "+path.u8string(); return result; }
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

    // unit_meters is the file's active UnitScaleFactor, used by ufbx's conversion.
    // original_unit_meters can describe a previous authoring unit instead.
    const double sourceUnitMeters = scene->settings.unit_meters;
    if (ufbx_find_prop(&scene->settings.props, "UnitScaleFactor") != nullptr &&
        std::isfinite(sourceUnitMeters) && sourceUnitMeters > 0.0)
        result.model.sourceUnitMeters = sourceUnitMeters;
    else
        result.model.warnings.push_back("Source unit metadata is unavailable; ufbx default unit interpretation is used, with target meters.");

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
                0.0f, 1.0f);
        if (material->pbr.metalness.has_value)
            importedMaterial.metallic = std::clamp(
                static_cast<float>(material->pbr.metalness.value_real),
                0.0f, 1.0f);
        if (material->pbr.ambient_occlusion.has_value)
            importedMaterial.ambientOcclusion = std::clamp(
                static_cast<float>(material->pbr.ambient_occlusion.value_real),
                0.0f, 1.0f);
        if (material->pbr.opacity.has_value)
            importedMaterial.opacity = std::clamp(
                static_cast<float>(material->pbr.opacity.value_real),
                0.0f, 1.0f);

        const auto& baseMap=baseColor.texture ? baseColor : material->fbx.diffuse_color;
        const ufbx_texture* baseTexture=SupportedTexture(result.model,*scene,*material,baseMap,"base color");
        importedMaterial.baseColorTexture = ImportTexture(
            result.model, baseTexture, textureIndices);
        ImportPbrTextures(result.model,*scene,*material,options,importedMaterial,textureIndices);

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
            {
                if (!warning.empty())
                {
                    result.error = std::move(warning);
                    return result;
                }
                continue;
            }
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
    const cy::Vec3f size = result.model.boundsMax - result.model.boundsMin;
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
        size.x < 0.0f || size.y < 0.0f || size.z < 0.0f)
        result.error = "The FBX produced invalid converted model dimensions.";
    if (result.model.meshes.empty())
        result.error = "The FBX contains no renderable triangle sections.";

    return result;
}
}
