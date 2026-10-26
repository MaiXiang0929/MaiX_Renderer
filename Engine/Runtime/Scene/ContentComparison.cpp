// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/ContentComparison.h"
#include <algorithm>

namespace {
bool Same(const cy::Vec3f& a,const cy::Vec3f& b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
bool Same(const Transform& a,const Transform& b)
{ return Same(a.position,b.position)&&Same(a.rotationDegrees,b.rotationDegrees)&&Same(a.scale,b.scale); }
bool Same(const SceneAssetPath& a,const SceneAssetPath& b) { return a.root==b.root&&a.path==b.path; }
template<class T,class Equal> bool SameOptional(const std::optional<T>& a,const std::optional<T>& b,Equal equal)
{ return a.has_value()==b.has_value()&&(!a||equal(*a,*b)); }
bool Same(const SceneTextureRecord& a,const SceneTextureRecord& b)
{
    if(a.mode!=b.mode||a.embeddedId!=b.embeddedId||!SameOptional(a.source,b.source,[](const auto& x,const auto& y) { return Same(x,y); })) return false;
    return SameOptional(a.recipe,b.recipe,[](const auto& x,const auto& y) {
        if(x.kind!=y.kind||x.flipGreen!=y.flipGreen||x.width!=y.width||x.height!=y.height) return false;
        for(std::size_t i=0;i<3;++i) if(!SameOptional(x.ormInputs[i],y.ormInputs[i],[](const auto& p,const auto& q) {
            return p.embeddedId==q.embeddedId&&p.channel==q.channel&&Same(p.source,q.source);
        })) return false;
        return true;
    });
}
bool Same(const MaterialProperties& a,const MaterialProperties& b)
{
#define FIELD(name) if(a.name!=b.name) return false;
    FIELD(shininess) FIELD(environmentReflectivity) FIELD(metallic) FIELD(roughness)
    FIELD(ambientOcclusion) FIELD(normalScale) FIELD(opacity) FIELD(alphaCutoff) FIELD(shadingModel) FIELD(surfaceSide)
    FIELD(toonThreshold) FIELD(toonShadowStrength) FIELD(rimLightStrength) FIELD(faceShadowEnabled)
    FIELD(faceShadowSoftness) FIELD(faceShadowMirrorX) FIELD(outlineEnabled) FIELD(outlineThickness) FIELD(emissiveIntensity)
#undef FIELD
#define FIELD(name) if(!Same(a.name,b.name)) return false;
    FIELD(baseColor) FIELD(specularColor) FIELD(toonShadowColor) FIELD(rimLightColor)
    FIELD(faceForwardLocal) FIELD(faceRightLocal) FIELD(outlineColor) FIELD(emissiveColor)
#undef FIELD
    return true;
}
bool Same(const SceneSectionRecord& a,const SceneSectionRecord& b)
{
    return a.mesh==b.mesh&&a.name==b.name&&a.material==b.material&&a.castsShadow==b.castsShadow&&
        a.bounds.radius==b.bounds.radius&&Same(a.bounds.center,b.bounds.center)&&std::equal(a.local.cell,a.local.cell+16,b.local.cell);
}
bool Same(const SceneModelRecord& a,const SceneModelRecord& b)
{
    if(a.cubeEdge!=b.cubeEdge||a.meshNames!=b.meshNames||a.sections.size()!=b.sections.size()||
        !SameOptional(a.source,b.source,[](const auto& x,const auto& y) { return Same(x,y); })) return false;
    for(std::size_t i=0;i<a.sections.size();++i) if(!Same(a.sections[i],b.sections[i])) return false;
    return true;
}
bool Same(const LightSceneProxy& a,const LightSceneProxy& b)
{
#define FIELD(name) if(a.name!=b.name) return false;
    FIELD(type) FIELD(falloff) FIELD(intensity) FIELD(range) FIELD(innerConeAngle) FIELD(outerConeAngle)
    FIELD(castsShadow) FIELD(shadowBias) FIELD(shadowNormalBias) FIELD(cascadeCount) FIELD(shadowDistance) FIELD(splitLambda)
#undef FIELD
    return Same(a.color,b.color);
}
bool Same(const SceneActorRecord& a,const SceneActorRecord& b)
{
    return a.id==b.id&&a.parent==b.parent&&a.name==b.name&&a.visible==b.visible&&Same(a.local,b.local)&&
        SameOptional(a.model,b.model,[](const auto& x,const auto& y) { return Same(x,y); })&&
        SameOptional(a.light,b.light,[](const auto& x,const auto& y) { return Same(x,y); })&&
        SameOptional(a.camera,b.camera,[](const auto& x,const auto& y) { return x.fovDegrees==y.fovDegrees&&x.nearPlane==y.nearPlane&&x.farPlane==y.farPlane; });
}
bool Same(const SceneMaterialRecord& a,const SceneMaterialRecord& b)
{
    if(a.id!=b.id||a.parent!=b.parent||a.name!=b.name||a.overrides!=b.overrides||a.blend!=b.blend||!Same(a.properties,b.properties)) return false;
    for(std::size_t i=0;i<MaterialTextureSlotCount;++i) if(!Same(a.textures[i],b.textures[i])) return false;
    return true;
}
bool KeysEqual(const std::vector<ContentRecordKey>& a,const std::vector<ContentRecordKey>& b)
{
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(a[i].key!=b[i].key||a[i].guid!=b[i].guid||a[i].shared!=b[i].shared) return false;
    return true;
}
}
bool RegionalContentEqual(const ContentDocument& a,const ContentDocument& b)
{
    if(!KeysEqual(a.actors,b.actors)||!KeysEqual(a.materials,b.materials)||a.scene.actors.size()!=b.scene.actors.size()||a.scene.materials.size()!=b.scene.materials.size()) return false;
    for(std::size_t i=0;i<a.scene.actors.size();++i) if(!Same(a.scene.actors[i],b.scene.actors[i])) return false;
    for(std::size_t i=0;i<a.scene.materials.size();++i) if(!Same(a.scene.materials[i],b.scene.materials[i])) return false;
    return true;
}
