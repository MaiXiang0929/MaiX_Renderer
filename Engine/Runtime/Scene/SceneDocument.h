// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/World.h"
#include "Engine/Core/Math/Bounds.h"
#include "Engine/Runtime/Scene/SceneCamera.h"
#include "Engine/Rendering/Material/MaterialProperties.h"
#include "Engine/Rendering/Renderer/Scene/LightSceneProxy.h"
#include "Engine/Rendering/Renderer/Pipeline/RenderSettings.h"
#include "Engine/Asset/Importer/TextureSource.h"
#include <filesystem>
#include <array>
#include <optional>

// File-local keys are deliberately unrelated to runtime Actor/RenderResource handles.
struct SceneAssetPath
{
    std::string root = "project"; // project Assets, engine assets, or explicit external path
    std::string path;
};
struct SceneAssetRoots
{
    std::filesystem::path projectAssets, engineAssets, engineSourceAssets;
};
struct SceneTextureChannelInput
{
    SceneAssetPath source;
    std::uint64_t embeddedId = 0;
    unsigned channel = 0;
};
enum class SceneTextureRecipeKind { Normal, Orm };
struct SceneTextureRecipe
{
    SceneTextureRecipeKind kind = SceneTextureRecipeKind::Normal;
    bool flipGreen = false;
    std::array<std::optional<SceneTextureChannelInput>,3> ormInputs;
    unsigned width = 0, height = 0;
};
struct SceneTextureRecord
{
    TextureOverrideMode mode = TextureOverrideMode::Inherit;
    std::optional<SceneAssetPath> source;
    std::uint64_t embeddedId = 0; // Stable source identity; zero denotes a standalone image.
    std::optional<SceneTextureRecipe> recipe;
};
struct SceneMaterialRecord
{
    std::uint64_t id = 0, parent = 0;
    std::string name;
    MaterialProperties properties;
    MaterialOverrideMask overrides;
    BlendMode blend = BlendMode::Opaque;
    std::array<SceneTextureRecord, MaterialTextureSlotCount> textures;
};
struct SceneSectionRecord
{
    std::size_t mesh = 0;
    std::string name;
    std::uint64_t material = 0;
    cy::Matrix4f local = cy::Matrix4f::Identity();
    PrimitiveBounds bounds;
    bool castsShadow = true;
};
struct SceneModelRecord
{
    std::optional<SceneAssetPath> source;
    float cubeEdge = 0; // Existing built-in cube only; no generic geometry serialization.
    std::vector<std::string> meshNames;
    std::vector<SceneSectionRecord> sections;
};
struct SceneActorRecord
{
    std::uint64_t id = 0, parent = 0;
    std::string name;
    Transform local;
    bool visible = true;
    std::optional<SceneModelRecord> model;
    std::optional<LightSceneProxy> light;
    std::optional<SceneCamera> camera;
};
struct SceneDocument
{
    PostProcessSettings postProcess;
    ShadowSettings shadowSettings;
    EnvironmentSettings environment;
    std::optional<SceneAssetPath> environmentSource;
    std::vector<SceneActorRecord> actors;
    std::vector<SceneMaterialRecord> materials;
    std::uint64_t defaultModel = 0, mainLight = 0;
    bool reflectionGroundEnabled = false;
    float groundY = 0, modelDiameter = 0, sceneRadius = 1;
    cy::Vec3f defaultBoundsMin{0.0f}, defaultBoundsMax{0.0f}, objCenter{0.0f};
};
constexpr std::size_t MaximumSceneDocumentBytes = 64 * 1024 * 1024;
bool ValidateSceneDocument(const SceneDocument&, std::string& error);
// Throws on duplicate identities, missing parents or cycles; used before World mutations.
std::vector<std::size_t> SceneActorLoadOrder(const SceneDocument&);
bool EncodeSceneDocument(const SceneDocument&, std::string& text, std::string& error);
bool DecodeSceneDocument(const std::string& text, SceneDocument&, std::string& error);
bool ReadSceneDocument(const std::filesystem::path&, SceneDocument&, std::string& error);
bool WriteSceneDocument(const std::filesystem::path&, const SceneDocument&, std::string& error, bool overwrite = true);
SceneAssetPath MakeSceneAssetPath(const std::filesystem::path&, const SceneAssetRoots&);
std::filesystem::path ResolveSceneAssetPath(const SceneAssetPath&, const SceneAssetRoots&);
// Convert every reconstruction dependency through the explicit project/engine/external roots.
SceneTextureRecord MakeSceneTextureRecord(const AssetImport::TextureSource&, const SceneAssetRoots&);
AssetImport::TextureSource ResolveSceneTextureSource(const SceneTextureRecord&, const SceneAssetRoots&);
