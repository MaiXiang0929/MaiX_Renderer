// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/SceneDocument.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace
{
using J = nlohmann::json;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void Fields(const J& j,std::initializer_list<const char*> allowed)
{
    Check(j.is_object(),"Expected a JSON object.");
    for (auto field=j.begin();field!=j.end();++field)
    {
        bool known=false; for (const auto* name:allowed) if (field.key()==name) { known=true; break; }
        if (!known) throw std::runtime_error("Unknown scene field/component: "+field.key());
    }
}
J Vec(const cy::Vec3f& v) { return J::array({v.x,v.y,v.z}); }
cy::Vec3f Vector(const J& j)
{
    Check(j.is_array() && j.size()==3, "Expected a three-component vector.");
    cy::Vec3f v;
    for (int i=0;i<3;++i)
    {
        Check(j[i].is_number(), "Vector component must be numeric.");
        v[i]=j[i].get<float>(); Check(std::isfinite(v[i]), "Non-finite vector.");
    }
    return v;
}
float Number(const J& j)
{
    Check(j.is_number(), "Expected a number.");
    float v=j.get<float>(); Check(std::isfinite(v), "Non-finite number."); return v;
}
std::uint64_t Key(const J& j)
{
    Check(j.is_number_integer() && (j.is_number_unsigned() || j.get<std::int64_t>()>=0),
        "Expected a non-negative integer key.");
    return j.get<std::uint64_t>();
}
bool Finite(const cy::Vec3f& v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
J TransformJson(const Transform& t)
{ return {{"position",Vec(t.position)},{"rotation",Vec(t.rotationDegrees)},{"scale",Vec(t.scale)}}; }
Transform ReadTransform(const J& j)
{
    Fields(j,{"position","rotation","scale"});
    Transform t; t.position=Vector(j.at("position")); t.rotationDegrees=Vector(j.at("rotation"));
    t.scale=Vector(j.at("scale")); return t;
}
J Asset(const SceneAssetPath& p) { return {{"root",p.root},{"path",p.path}}; }
SceneAssetPath ReadAsset(const J& j) { Fields(j,{"root","path"}); return {j.at("root").get<std::string>(),j.at("path").get<std::string>()}; }
void CheckAsset(const SceneAssetPath& p)
{
    const auto path=std::filesystem::u8path(p.path);
    Check(!p.path.empty() && p.path.find('\0')==std::string::npos, "Empty or invalid asset path.");
    Check(p.root=="project"||p.root=="engine"||p.root=="external", "Unknown asset root.");
    if (p.root=="external") Check(path.is_absolute(), "External asset path must be absolute.");
    else
    {
        Check(!path.has_root_path(), "Rooted path in relative asset reference.");
        for (const auto& part:path) Check(part!="..", "Asset path escapes its root.");
    }
}
#define SCENE_FLOATS(X) X(shininess) X(environmentReflectivity) X(metallic) X(roughness) \
    X(ambientOcclusion) X(normalScale) X(opacity) X(toonThreshold) X(toonShadowStrength) \
    X(rimLightStrength) X(faceShadowSoftness) X(outlineThickness)
#define SCENE_VECTORS(X) X(baseColor) X(specularColor) X(toonShadowColor) X(rimLightColor) \
    X(faceForwardLocal) X(faceRightLocal) X(outlineColor)
J Properties(const MaterialProperties& p)
{
    J j;
#define PUT_FLOAT(field) j[#field]=p.field;
    SCENE_FLOATS(PUT_FLOAT)
#undef PUT_FLOAT
#define PUT_VEC(field) j[#field]=Vec(p.field);
    SCENE_VECTORS(PUT_VEC)
#undef PUT_VEC
    j["alphaCutoff"]=p.alphaCutoff;
    j["surfaceSide"]=static_cast<int>(p.surfaceSide);
    j["shadingModel"]=static_cast<int>(p.shadingModel); j["faceShadowEnabled"]=p.faceShadowEnabled;
    j["faceShadowMirrorX"]=p.faceShadowMirrorX; j["outlineEnabled"]=p.outlineEnabled; return j;
}
MaterialProperties ReadProperties(const J& j, std::uint64_t version)
{
    Fields(j,{"baseColor","specularColor","shininess","environmentReflectivity","metallic","roughness",
        "ambientOcclusion","normalScale","opacity","alphaCutoff","shadingModel","toonThreshold","toonShadowStrength","toonShadowColor",
        "rimLightStrength","rimLightColor","faceShadowEnabled","faceForwardLocal","faceRightLocal","faceShadowSoftness",
        "faceShadowMirrorX","outlineEnabled","outlineThickness","outlineColor","surfaceSide"});
    MaterialProperties p;
    if (version>=4)
    {
        const auto side=Key(j.at("surfaceSide"));
        Check(side<=static_cast<std::uint64_t>(SurfaceSide::Legacy),"Unknown surface side.");
        p.surfaceSide=static_cast<SurfaceSide>(side);
    }
    else
    {
        Check(!j.contains("surfaceSide"),"Old scene versions cannot contain surface side.");
        p.surfaceSide=SurfaceSide::Legacy;
    }
#define GET_FLOAT(field) p.field=Number(j.at(#field));
    SCENE_FLOATS(GET_FLOAT)
#undef GET_FLOAT
#define GET_VEC(field) p.field=Vector(j.at(#field));
    SCENE_VECTORS(GET_VEC)
#undef GET_VEC
    if (j.contains("alphaCutoff")) p.alphaCutoff=Number(j.at("alphaCutoff"));
    const auto shading=Key(j.at("shadingModel")); Check(shading<=1,"Unknown shading model.");
    p.shadingModel=static_cast<ShadingModel>(shading);
    p.faceShadowEnabled=j.at("faceShadowEnabled").get<bool>();
    p.faceShadowMirrorX=j.at("faceShadowMirrorX").get<bool>();
    p.outlineEnabled=j.at("outlineEnabled").get<bool>(); return p;
}
bool Under(const std::filesystem::path& file,const std::filesystem::path& root)
{
    if (root.empty()) return false;
    auto f=file.begin();
    for (auto r=root.begin();r!=root.end();++r,++f)
    {
        if (f==file.end()) return false;
#ifdef _WIN32
        if (_wcsicmp(f->c_str(),r->c_str())!=0) return false;
#else
        if (*f!=*r) return false;
#endif
    }
    return f!=file.end();
}
}

std::vector<std::size_t> SceneActorLoadOrder(const SceneDocument& d)
{
    std::unordered_map<std::uint64_t,size_t> indices;
    std::vector<std::vector<size_t>> children(d.actors.size());
    std::vector<size_t> order; order.reserve(d.actors.size());
    for (size_t i=0;i<d.actors.size();++i)
        Check(d.actors[i].id&&indices.emplace(d.actors[i].id,i).second,"Duplicate/zero actor key.");
    for (size_t i=0;i<d.actors.size();++i)
    {
        const auto parent=d.actors[i].parent;
        if (!parent) order.push_back(i);
        else
        {
            const auto p=indices.find(parent); Check(p!=indices.end(),"Missing actor parent.");
            children[p->second].push_back(i);
        }
    }
    for (size_t i=0;i<order.size();++i)
        for (const auto child:children[order[i]]) order.push_back(child);
    Check(order.size()==d.actors.size(),"Scene hierarchy contains a cycle."); return order;
}

bool ValidateSceneDocument(const SceneDocument& d, std::string& error)
{
    try
    {
        Check(IsValidEnvironmentSettings(d.environment),"Invalid environment intensity or rotation.");
        if (d.environmentSource)
        {
            CheckAsset(*d.environmentSource);
            auto extension=std::filesystem::u8path(d.environmentSource->path).extension().u8string();
            std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            Check(extension==".hdr","Environment source must reference a Radiance .hdr file.");
        }
        Check(d.actors.size()<=10000 && d.materials.size()<=10000,"Scene record limit exceeded.");
        std::unordered_map<std::uint64_t, const SceneMaterialRecord*> materials;
        for (const auto& m:d.materials)
        {
            Check(m.id && materials.emplace(m.id,&m).second,"Duplicate or zero material key.");
            Check(static_cast<int>(m.blend)>=0 && static_cast<int>(m.blend)<=2,"Invalid blend mode.");
            Check(IsValidSurfaceSide(m.properties.surfaceSide),"Invalid surface side.");
            Check(std::isfinite(m.properties.alphaCutoff)&&m.properties.alphaCutoff>=0&&m.properties.alphaCutoff<=1,"Invalid alpha cutoff.");
            Check(static_cast<int>(m.properties.shadingModel)>=0 && static_cast<int>(m.properties.shadingModel)<=1,"Invalid shading model.");
#define CHECK_FLOAT(field) Check(std::isfinite(m.properties.field),"Non-finite material parameter.");
            SCENE_FLOATS(CHECK_FLOAT)
#undef CHECK_FLOAT
#define CHECK_VEC(field) Check(Finite(m.properties.field),"Non-finite material vector.");
            SCENE_VECTORS(CHECK_VEC)
#undef CHECK_VEC
            for (const auto& t:m.textures)
            {
                Check(static_cast<int>(t.mode)>=0 && static_cast<int>(t.mode)<=2,"Invalid texture override mode.");
                if (t.source) CheckAsset(*t.source);
                Check(t.embeddedId==0 || t.source.has_value(),"Embedded texture needs its source FBX.");
                Check(t.mode!=TextureOverrideMode::Replace || t.source.has_value(),"Replacement texture has no source.");
                Check(!m.parent || t.mode==TextureOverrideMode::Replace || !t.source,"Inherited/disabled texture must not have its own source.");
                Check(m.parent || t.mode==TextureOverrideMode::Inherit,"Base material cannot contain texture overrides.");
            }
        }
        for (const auto& m:d.materials) if (m.parent)
        {
            auto p=materials.find(m.parent);
            Check(p!=materials.end() && p->second->parent==0 && m.parent!=m.id,"Invalid material parent; only single-level instances are supported.");
        }
        World check;
        std::unordered_map<std::uint64_t,ActorId> actors;
        std::unordered_map<std::uint64_t,const SceneActorRecord*> records;
        unsigned cameras=0;
        for (const auto& a:d.actors)
        {
            Check(a.id && !a.name.empty() && records.emplace(a.id,&a).second,"Duplicate/zero actor key or empty name.");
            Check(static_cast<int>(a.model.has_value())+a.light.has_value()+a.camera.has_value()<=1,"Actor has conflicting typed components.");
            Check(Finite(a.local.position)&&Finite(a.local.rotationDegrees)&&Finite(a.local.scale),"Non-finite local transform.");
            auto id=check.Create(a.name,a.local); Check(id.IsValid(),"Invalid local TRS."); actors.emplace(a.id,id);
            if (a.model)
            {
                const auto& m=*a.model;
                Check(m.source.has_value() != (m.cubeEdge>0),"Model needs exactly one supported source.");
                Check(std::isfinite(m.cubeEdge)&&m.cubeEdge>=0,"Invalid cube size.");
                if (m.source) CheckAsset(*m.source);
                Check(!m.meshNames.empty()&&!m.sections.empty()&&m.meshNames.size()<=100000&&m.sections.size()<=100000,"Invalid model mesh/section counts.");
                for (const auto& s:m.sections)
                {
                    Check(s.mesh<m.meshNames.size()&&materials.count(s.material),"Invalid section mesh/material reference.");
                    for (int i=0;i<16;++i) Check(std::isfinite(s.local.cell[i]),"Non-finite section matrix.");
                    Check(Finite(s.bounds.center)&&std::isfinite(s.bounds.radius)&&s.bounds.radius>=0,"Invalid section bounds.");
                }
            }
            if (a.light)
            {
                const auto& l=*a.light;
                Check(static_cast<int>(l.type)>=0&&static_cast<int>(l.type)<=2,"Invalid light type.");
                Check(IsValidLightShadowSettings(l),"Invalid light shadow settings.");
                Check(Finite(l.color)&&l.color.x>=0&&l.color.y>=0&&l.color.z>=0,"Invalid light color.");
                Check(std::isfinite(l.intensity)&&l.intensity>=0&&std::isfinite(l.range)&&l.range>0,"Invalid light intensity/range.");
                Check(std::isfinite(l.innerConeAngle)&&std::isfinite(l.outerConeAngle)&&l.innerConeAngle>=0&&l.outerConeAngle>0&&l.innerConeAngle<=l.outerConeAngle&&l.outerConeAngle<1.5707964f,"Invalid spotlight cone.");
            }
            if (a.camera)
            {
                ++cameras; const auto& c=*a.camera;
                Check(std::isfinite(c.fovDegrees)&&c.fovDegrees>0&&c.fovDegrees<180&&std::isfinite(c.nearPlane)&&c.nearPlane>0&&std::isfinite(c.farPlane)&&c.farPlane>c.nearPlane,"Invalid camera projection.");
            }
        }
        Check(cameras<=1,"Only one scene camera is currently supported.");
        // Parents are resolved independently of file order. World remains the TRS authority.
        for (const auto index:SceneActorLoadOrder(d))
        {
            const auto& a=d.actors[index]; if (!a.parent) continue;
            Check(actors.count(a.parent),"Missing actor parent.");
            if (!check.Reparent(actors.at(a.id),actors.at(a.parent),false,error)) throw std::runtime_error(error);
        }
        Check(!d.defaultModel || (records.count(d.defaultModel)&&records.at(d.defaultModel)->model),"Invalid default model reference.");
        Check(!d.mainLight || (records.count(d.mainLight)&&records.at(d.mainLight)->light),"Invalid main light reference.");
        Check(std::isfinite(d.groundY)&&std::isfinite(d.sceneRadius)&&d.sceneRadius>0&&std::isfinite(d.modelDiameter)&&d.modelDiameter>=0&&Finite(d.defaultBoundsMin)&&Finite(d.defaultBoundsMax)&&Finite(d.objCenter),"Invalid scene framing values.");
        Check(IsValidPostProcessSettings(d.postProcess),"Invalid scene post-process settings (non-finite or out of range).");
        Check(IsValidShadowSettings(d.shadowSettings),"Invalid scene shadow settings.");
        error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}

bool EncodeSceneDocument(const SceneDocument& d,std::string& text,std::string& error)
{
    if (!ValidateSceneDocument(d,error)) return false;
    try
    {
        J j={{"format","MaiX Scene"},{"version",4},{"actors",J::array()},{"materials",J::array()},
            {"defaultModel",d.defaultModel},{"mainLight",d.mainLight},{"reflectionGroundEnabled",d.reflectionGroundEnabled},
            {"groundY",d.groundY},{"modelDiameter",d.modelDiameter},{"sceneRadius",d.sceneRadius},
            {"defaultBoundsMin",Vec(d.defaultBoundsMin)},{"defaultBoundsMax",Vec(d.defaultBoundsMax)},{"objCenter",Vec(d.objCenter)}};
        j["environment"]={{"source",d.environmentSource?Asset(*d.environmentSource):J(nullptr)},
            {"intensity",d.environment.intensity},{"rotationDegrees",d.environment.rotationDegrees}};
        const auto& p=d.postProcess;
        const auto& shadows=d.shadowSettings;
        j["shadows"]={{"multiLight",shadows.multiLight},{"maxLights",shadows.maxLights},
            {"resolution",shadows.resolution},{"pointResolution",shadows.pointResolution}};
        j["postProcess"]={{"fxaaEnabled",p.fxaaEnabled},{"toneMappingEnabled",p.toneMappingEnabled},{"exposureCompensation",p.exposureCompensation},
            {"bloomEnabled",p.bloomEnabled},{"bloomThreshold",p.bloomThreshold},{"bloomIntensity",p.bloomIntensity},
            {"ssaoEnabled",p.ssaoEnabled},{"ssaoRadius",p.ssaoRadius},{"ssaoIntensity",p.ssaoIntensity},{"ssaoBias",p.ssaoBias}};
        for (const auto& m:d.materials)
        {
            J value={{"id",m.id},{"parent",m.parent},{"name",m.name},{"properties",Properties(m.properties)},
                {"overrides",m.overrides.to_string()},{"blend",static_cast<int>(m.blend)},{"textures",J::array()}};
            for (const auto& t:m.textures)
                value["textures"].push_back({{"mode",static_cast<int>(t.mode)},
                    {"source",t.source?Asset(*t.source):J(nullptr)},{"embeddedId",t.embeddedId}});
            j["materials"].push_back(std::move(value));
        }
        for (const auto& a:d.actors)
        {
            J value={{"id",a.id},{"parent",a.parent},{"name",a.name},{"local",TransformJson(a.local)},{"visible",a.visible}};
            if (a.model)
            {
                const auto& m=*a.model;
                J model={{"source",m.source?Asset(*m.source):J(nullptr)},{"cubeEdge",m.cubeEdge},{"meshNames",m.meshNames},{"sections",J::array()}};
                for (const auto& s:m.sections)
                {
                    J matrix=J::array(); for (int i=0;i<16;++i) matrix.push_back(s.local.cell[i]);
                    model["sections"].push_back({{"mesh",s.mesh},{"name",s.name},{"material",s.material},{"local",matrix},
                        {"center",Vec(s.bounds.center)},{"radius",s.bounds.radius},{"castsShadow",s.castsShadow}});
                }
                value["model"]=std::move(model);
            }
            if (a.light)
            {
                const auto& l=*a.light;
                value["light"]={{"type",static_cast<int>(l.type)},{"color",Vec(l.color)},{"intensity",l.intensity},{"range",l.range},
                    {"innerConeAngle",l.innerConeAngle},{"outerConeAngle",l.outerConeAngle},{"castsShadow",l.castsShadow},
                    {"shadowBias",l.shadowBias},{"shadowNormalBias",l.shadowNormalBias},{"cascadeCount",l.cascadeCount},
                    {"shadowDistance",l.shadowDistance},{"splitLambda",l.splitLambda}};
            }
            if (a.camera) value["camera"]={{"fovDegrees",a.camera->fovDegrees},{"nearPlane",a.camera->nearPlane},{"farPlane",a.camera->farPlane}};
            j["actors"].push_back(std::move(value));
        }
        auto encoded=j.dump(2);
        Check(encoded.size()<=MaximumSceneDocumentBytes,"Scene file exceeds 64 MiB.");
        text=std::move(encoded); error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}

bool DecodeSceneDocument(const std::string& text,SceneDocument& output,std::string& error)
{
    try
    {
        Check(text.size()<=MaximumSceneDocumentBytes,"Scene file exceeds 64 MiB.");
        std::vector<std::unordered_set<std::string>> keys;
        const auto j=J::parse(text,[&](int depth,J::parse_event_t event,J& value) {
            Check(depth<=128,"Scene JSON nesting limit exceeded.");
            if (event==J::parse_event_t::object_start) keys.emplace_back();
            if (event==J::parse_event_t::key)
                Check(!keys.empty()&&keys.back().insert(value.get<std::string>()).second,"Duplicate JSON key.");
            if (event==J::parse_event_t::object_end) keys.pop_back();
            return true;
        });
        Fields(j,{"format","version","actors","materials","defaultModel","mainLight","reflectionGroundEnabled","groundY",
            "modelDiameter","sceneRadius","defaultBoundsMin","defaultBoundsMax","objCenter","postProcess","environment","shadows"});
        const auto version=Key(j.at("version"));
        Check(j.at("format")=="MaiX Scene"&&(version>=1&&version<=4),"Unsupported scene format/version.");
        SceneDocument d;
        if (version>=3)
        {
            const auto& shadows=j.at("shadows"); Fields(shadows,{"multiLight","maxLights","resolution","pointResolution"});
            d.shadowSettings.multiLight=shadows.at("multiLight").get<bool>();
            const auto maxLights=Key(shadows.at("maxLights")), resolution=Key(shadows.at("resolution")), pointResolution=Key(shadows.at("pointResolution"));
            Check(maxLights>=1&&maxLights<=4&&resolution<=2048&&pointResolution<=2048,"Invalid shadow budget or resolution.");
            d.shadowSettings.maxLights=static_cast<unsigned>(maxLights);
            d.shadowSettings.resolution=static_cast<unsigned>(resolution);
            d.shadowSettings.pointResolution=static_cast<unsigned>(pointResolution);
        }
        else
        {
            Check(!j.contains("shadows"),"Old scene versions cannot contain shadow settings.");
            d.shadowSettings.multiLight=false; d.shadowSettings.maxLights=1;
            d.shadowSettings.resolution=2048;
        }
        if (j.contains("environment"))
        {
            const auto& env=j.at("environment"); Fields(env,{"source","intensity","rotationDegrees"});
            d.environment.intensity=Number(env.at("intensity"));
            d.environment.rotationDegrees=Number(env.at("rotationDegrees"));
            if (!env.at("source").is_null()) d.environmentSource=ReadAsset(env.at("source"));
        }
        d.postProcess.fxaaEnabled=false; // Old v1/v2 scenes retain their authored appearance.
        if (version>=2)
        {
            const auto& p=j.at("postProcess");
            Fields(p,{"fxaaEnabled","toneMappingEnabled","exposureCompensation","bloomEnabled","bloomThreshold","bloomIntensity",
                "ssaoEnabled","ssaoRadius","ssaoIntensity","ssaoBias"});
            if (p.contains("fxaaEnabled")) d.postProcess.fxaaEnabled=p.at("fxaaEnabled").get<bool>();
            d.postProcess.toneMappingEnabled=p.at("toneMappingEnabled").get<bool>();
            d.postProcess.exposureCompensation=Number(p.at("exposureCompensation"));
            d.postProcess.bloomEnabled=p.at("bloomEnabled").get<bool>();
            d.postProcess.bloomThreshold=Number(p.at("bloomThreshold"));
            d.postProcess.bloomIntensity=Number(p.at("bloomIntensity"));
            d.postProcess.ssaoEnabled=p.at("ssaoEnabled").get<bool>();
            d.postProcess.ssaoRadius=Number(p.at("ssaoRadius"));
            d.postProcess.ssaoIntensity=Number(p.at("ssaoIntensity"));
            d.postProcess.ssaoBias=Number(p.at("ssaoBias"));
        }
        else Check(!j.contains("postProcess"),"Version 1 scenes cannot contain post-process settings.");
        const auto& actors=j.at("actors"); const auto& materials=j.at("materials");
        Check(actors.is_array()&&materials.is_array()&&actors.size()<=10000&&materials.size()<=10000,"Invalid scene record arrays.");
        for (const auto& v:materials)
        {
            Fields(v,{"id","parent","name","properties","overrides","blend","textures"});
            SceneMaterialRecord m; m.id=Key(v.at("id")); m.parent=Key(v.at("parent")); m.name=v.at("name").get<std::string>();
            m.properties=ReadProperties(v.at("properties"),version);
            const auto mask=v.at("overrides").get<std::string>(); Check((mask.size()==MaterialParameterCount||mask.size()==MaterialParameterCount-1)&&mask.find_first_not_of("01")==std::string::npos,"Invalid material override mask.");
            m.overrides=MaterialOverrideMask(mask);
            const auto blend=Key(v.at("blend")); Check(blend<=2,"Invalid blend mode."); m.blend=static_cast<BlendMode>(blend);
            const auto& ts=v.at("textures"); Check(ts.is_array()&&ts.size()==MaterialTextureSlotCount,"Invalid texture slots.");
            for (size_t i=0;i<ts.size();++i)
            {
                Fields(ts[i],{"mode","source","embeddedId"});
                const auto mode=Key(ts[i].at("mode")); Check(mode<=2,"Invalid texture override mode.");
                auto& t=m.textures[i]; t.mode=static_cast<TextureOverrideMode>(mode);
                if (!ts[i].at("source").is_null()) t.source=ReadAsset(ts[i].at("source"));
                t.embeddedId=Key(ts[i].at("embeddedId"));
            }
            d.materials.push_back(std::move(m));
        }
        for (const auto& v:actors)
        {
            Fields(v,{"id","parent","name","local","visible","model","light","camera"});
            SceneActorRecord a; a.id=Key(v.at("id")); a.parent=Key(v.at("parent"));
            a.name=v.at("name").get<std::string>(); a.local=ReadTransform(v.at("local")); a.visible=v.at("visible").get<bool>();
            if (v.contains("model"))
            {
                a.model.emplace(); auto& m=*a.model; const auto& model=v.at("model");
                Fields(model,{"source","cubeEdge","meshNames","sections"});
                if (!model.at("source").is_null()) m.source=ReadAsset(model.at("source"));
                m.cubeEdge=Number(model.at("cubeEdge")); m.meshNames=model.at("meshNames").get<std::vector<std::string>>();
                const auto& sections=model.at("sections"); Check(sections.is_array()&&sections.size()<=100000,"Invalid model sections.");
                for (const auto& value:sections)
                {
                    Fields(value,{"mesh","material","name","local","center","radius","castsShadow"});
                    SceneSectionRecord s; s.mesh=Key(value.at("mesh")); s.material=Key(value.at("material"));
                    s.name=value.at("name").get<std::string>(); const auto& matrix=value.at("local");
                    Check(matrix.is_array()&&matrix.size()==16,"Invalid section matrix.");
                    for (int i=0;i<16;++i) s.local.cell[i]=Number(matrix[i]);
                    s.bounds.center=Vector(value.at("center")); s.bounds.radius=Number(value.at("radius"));
                    s.castsShadow=value.at("castsShadow").get<bool>(); m.sections.push_back(std::move(s));
                }
            }
            if (v.contains("light"))
            {
                a.light.emplace(); auto& l=*a.light; const auto& light=v.at("light");
                if (version>=3)
                    Fields(light,{"type","color","intensity","range","innerConeAngle","outerConeAngle","castsShadow",
                        "shadowBias","shadowNormalBias","cascadeCount","shadowDistance","splitLambda"});
                else Fields(light,{"type","color","intensity","range","innerConeAngle","outerConeAngle","castsShadow"});
                const auto type=Key(light.at("type")); Check(type<=2,"Invalid light type."); l.type=static_cast<LightType>(type);
                l.color=Vector(light.at("color")); l.intensity=Number(light.at("intensity")); l.range=Number(light.at("range"));
                l.innerConeAngle=Number(light.at("innerConeAngle")); l.outerConeAngle=Number(light.at("outerConeAngle"));
                l.castsShadow=light.at("castsShadow").get<bool>();
                if (version>=3)
                {
                    l.shadowBias=Number(light.at("shadowBias")); l.shadowNormalBias=Number(light.at("shadowNormalBias"));
                    const auto cascades=Key(light.at("cascadeCount")); Check(cascades==1||cascades==2||cascades==4,"Invalid cascade count.");
                    l.cascadeCount=static_cast<unsigned>(cascades);
                    l.shadowDistance=Number(light.at("shadowDistance")); l.splitLambda=Number(light.at("splitLambda"));
                }
                else { l.cascadeCount=1; if (l.type==LightType::Point) l.castsShadow=false; }
            }
            if (v.contains("camera"))
            {
                a.camera.emplace(); const auto& c=v.at("camera");
                Fields(c,{"fovDegrees","nearPlane","farPlane"});
                a.camera->fovDegrees=Number(c.at("fovDegrees")); a.camera->nearPlane=Number(c.at("nearPlane")); a.camera->farPlane=Number(c.at("farPlane"));
            }
            d.actors.push_back(std::move(a));
        }
        d.defaultModel=Key(j.at("defaultModel")); d.mainLight=Key(j.at("mainLight"));
        d.reflectionGroundEnabled=j.at("reflectionGroundEnabled").get<bool>();
        d.groundY=Number(j.at("groundY")); d.modelDiameter=Number(j.at("modelDiameter")); d.sceneRadius=Number(j.at("sceneRadius"));
        d.defaultBoundsMin=Vector(j.at("defaultBoundsMin")); d.defaultBoundsMax=Vector(j.at("defaultBoundsMax")); d.objCenter=Vector(j.at("objCenter"));
        if (!ValidateSceneDocument(d,error)) return false;
        output=std::move(d); error.clear(); return true;
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}

SceneAssetPath MakeSceneAssetPath(const std::filesystem::path& file,const SceneAssetRoots& roots)
{
    const auto path=std::filesystem::weakly_canonical(std::filesystem::absolute(file));
    for (const auto& pair:{std::make_pair("project",roots.projectAssets),std::make_pair("engine",roots.engineAssets),
        std::make_pair("engine",roots.engineSourceAssets)})
    {
        if (pair.second.empty()) continue;
        const auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(pair.second));
        if (Under(path,root))
        {
            // Windows component comparisons ignore case; lexically_relative does not.
            auto part=path.begin(); for (auto r=root.begin();r!=root.end();++r) ++part;
            std::filesystem::path relative; for (;part!=path.end();++part) relative/=*part;
            return {pair.first,relative.generic_u8string()};
        }
    }
    return {"external",path.generic_u8string()};
}
std::filesystem::path ResolveSceneAssetPath(const SceneAssetPath& ref,const SceneAssetRoots& roots)
{
    CheckAsset(ref);
    const auto relative=std::filesystem::u8path(ref.path);
    if (ref.root=="external") return relative;
    const auto& rootPath=ref.root=="project"?roots.projectAssets:roots.engineAssets;
    Check(!rootPath.empty(),"Asset root is unavailable.");
    const auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(rootPath));
    const auto result=std::filesystem::weakly_canonical(root/relative);
    Check(Under(result,root),"Asset path resolves outside its root."); return result;
}
