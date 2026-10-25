// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/WorldContentDocument.h"
#include "Engine/Core/Platform/Windows/AtomicFile.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <unordered_set>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace
{
using J=nlohmann::json;
void Check(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
J Parse(const std::string& text)
{
    Check(text.size()<=MaximumSceneDocumentBytes,"Content document exceeds 64 MiB.");
    std::vector<std::unordered_set<std::string>> keys;
    return J::parse(text,[&](int depth,J::parse_event_t event,J& value) {
        Check(depth<=128,"Content document nesting limit exceeded.");
        if (event==J::parse_event_t::object_start) keys.emplace_back();
        if (event==J::parse_event_t::key) Check(keys.back().insert(value.get<std::string>()).second,"Duplicate content field.");
        if (event==J::parse_event_t::object_end) keys.pop_back();
        return true;
    });
}
std::uint64_t Key(const J& value)
{
    Check(value.is_number_unsigned()||(value.is_number_integer()&&value.get<std::int64_t>()>0),"Content key must be a positive integer.");
    const auto key=value.get<std::uint64_t>(); Check(key!=0,"Zero content key."); return key;
}
J Keys(const std::vector<ContentRecordKey>& keys)
{
    J result=J::array();
    for (const auto& key:keys) result.push_back({{"key",key.key},{"guid",key.guid},{"shared",key.shared}});
    return result;
}
std::vector<ContentRecordKey> Keys(const J& value)
{
    Check(value.is_array()&&value.size()<=10000,"Invalid content identity table.");
    std::vector<ContentRecordKey> result; result.reserve(value.size());
    for (const auto& key:value) {
        Check(key.is_object()&&key.size()==3&&key.at("shared").is_boolean(),"Invalid content identity entry.");
        result.push_back({Key(key.at("key")),key.at("guid").get<std::string>(),key.at("shared").get<bool>()});
    }
    return result;
}
J Content(const ContentDocument& document)
{
    std::string text,error;
    Check(EncodeSceneDocument(document.scene,text,error),error);
    return {{"scene",J::parse(text)},{"actors",Keys(document.actors)},{"materials",Keys(document.materials)}};
}
ContentDocument Content(const J& value)
{
    Check(value.is_object()&&value.size()==3,"Invalid content body.");
    ContentDocument result; std::string error;
    Check(DecodeSceneDocument(value.at("scene").dump(),result.scene,error),error);
    result.actors=Keys(value.at("actors")); result.materials=Keys(value.at("materials"));
    return result;
}
void ValidateWorld(const WorldContentDocument& document)
{
    Check(IsContentGuid(document.id),"Invalid world GUID.");
    std::string error; Check(ValidateContentDocument(document.persistent,false,error),error);
    Check(document.regions.size()<=10000,"Region count limit exceeded.");
    std::unordered_set<std::string> ids,files;
    for (const auto& region:document.regions) {
        Check(IsContentGuid(region.id)&&region.id!=document.id&&ids.insert(region.id).second,"Invalid or duplicate region GUID.");
        Check(!region.name.empty()&&region.name.size()<=256,"Invalid region name.");
        const auto path=std::filesystem::u8path(region.source.path);
        Check(region.source.root=="project"&&!path.empty()&&!path.is_absolute()&&!path.has_root_name()&&
            path.extension()==".maixregion"&&region.source.path.find('\\')==std::string::npos&&
            region.source.path.find(':')==std::string::npos&&region.source.path.find('\0')==std::string::npos,"Region file must be an Assets-relative .maixregion path.");
        for (const auto& part:path) Check(part!=".."&&part!=".","Invalid region path segment.");
        auto key=path.generic_u8string();
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        Check(files.insert(key).second,"Duplicate region file.");
        for (int i=0;i<3;++i) Check(std::isfinite(region.minimum[i])&&std::isfinite(region.maximum[i])&&
            region.minimum[i]<region.maximum[i],"Region bounds must be finite and nonempty.");
    }
}
void ValidateRegion(const RegionContentDocument& document)
{
    Check(IsContentGuid(document.world)&&IsContentGuid(document.id)&&document.world!=document.id,"Invalid region/world GUID.");
    std::string error; Check(ValidateContentDocument(document.content,true,error),error);
}
std::string Read(const std::filesystem::path& file)
{
    std::ifstream input(file,std::ios::binary|std::ios::ate);
    Check(static_cast<bool>(input),"Cannot open content document: "+file.u8string());
    const auto size=input.tellg(); Check(size>0&&size<=static_cast<std::streamoff>(MaximumSceneDocumentBytes),"Invalid content file size.");
    std::string text(static_cast<std::size_t>(size),'\0'); input.seekg(0);
    Check(static_cast<bool>(input.read(text.data(),static_cast<std::streamsize>(size))),"Incomplete content document."); return text;
}
template<class F> bool Guard(std::string& error,F&& action)
{
    try { action(); error.clear(); return true; }
    catch(const std::exception& e) { error=e.what(); return false; }
}
}
bool ValidateContentDocument(const ContentDocument& document,bool region,std::string& error)
{
    return Guard(error,[&] {
        std::string message; Check(ValidateSceneDocument(document.scene,message),message);
        auto validate=[](const auto& records,const std::vector<ContentRecordKey>& keys,bool sharedAllowed) {
            Check(records.size()==keys.size(),"Content identities do not match records.");
            std::unordered_set<std::uint64_t> numbers; std::unordered_set<std::string> guids;
            for (const auto& key:keys) {
                Check(key.key&&IsContentGuid(key.guid)&&numbers.insert(key.key).second&&guids.insert(key.guid).second,"Invalid or duplicate content identity.");
                Check(!key.shared||sharedAllowed,"Shared identity is not allowed here.");
                Check(std::any_of(records.begin(),records.end(),[&](const auto& record) { return record.id==key.key; }),"Unknown content record key.");
            }
        };
        validate(document.scene.actors,document.actors,false);
        validate(document.scene.materials,document.materials,region);
        if (region) {
            Check(!document.scene.mainLight&&!document.scene.defaultModel,"World selections cannot be owned by a region.");
            for (const auto& actor:document.scene.actors) Check(!actor.camera,"Scene cameras must be persistent world content.");
            auto settings=document.scene; settings.actors.clear(); settings.materials.clear();
            std::string actual,defaults;
            Check(EncodeSceneDocument(settings,actual,message),message);
            Check(EncodeSceneDocument(SceneDocument{},defaults,message),message);
            Check(actual==defaults,"Regions cannot override world render/environment settings.");
        }
        for (const auto& key:document.materials) if (key.shared) {
            const auto& material=*std::find_if(document.scene.materials.begin(),document.scene.materials.end(),[&](const auto& m) { return m.id==key.key; });
            SceneDocument a,b; a.materials={material}; b.materials={SceneMaterialRecord{}};
            b.materials[0].id=key.key;
            std::string first,second;
            Check(EncodeSceneDocument(a,first,message)&&EncodeSceneDocument(b,second,message)&&first==second,"Shared material records must be value-free placeholders.");
        }
    });
}
bool EncodeWorldContentDocument(const WorldContentDocument& document,std::string& output,std::string& error)
{
    return Guard(error,[&] {
        ValidateWorld(document); J regions=J::array();
        for (const auto& r:document.regions) regions.push_back({{"id",r.id},{"name",r.name},{"file",r.source.path},
            {"minimum",{r.minimum.x,r.minimum.y,r.minimum.z}},{"maximum",{r.maximum.x,r.maximum.y,r.maximum.z}}});
        auto text=J{{"format","MaiX World"},{"version",1},{"id",document.id},{"persistent",Content(document.persistent)},{"regions",regions}}.dump(2);
        Check(text.size()<=MaximumSceneDocumentBytes,"World document exceeds 64 MiB."); output=std::move(text);
    });
}
bool DecodeWorldContentDocument(const std::string& text,WorldContentDocument& output,std::string& error)
{
    return Guard(error,[&] {
        const auto value=Parse(text);
        Check(value.is_object()&&value.size()==5&&value.at("format")=="MaiX World"&&value.at("version").is_number_integer()&&value.at("version")==1,"Unsupported world format/version.");
        WorldContentDocument document; document.id=value.at("id").get<std::string>(); document.persistent=Content(value.at("persistent"));
        Check(value.at("regions").is_array()&&value.at("regions").size()<=10000,"Invalid region table.");
        for (const auto& r:value.at("regions")) {
            Check(r.is_object()&&r.size()==5&&r.at("minimum").is_array()&&r.at("minimum").size()==3&&
                r.at("maximum").is_array()&&r.at("maximum").size()==3,"Invalid region description.");
            ContentRegionRecord record; record.id=r.at("id").get<std::string>(); record.name=r.at("name").get<std::string>();
            record.source={"project",r.at("file").get<std::string>()};
            for(int i=0;i<3;++i) { Check(r.at("minimum")[i].is_number()&&r.at("maximum")[i].is_number(),"Invalid bounds number.");
                record.minimum[i]=r.at("minimum")[i].get<float>(); record.maximum[i]=r.at("maximum")[i].get<float>(); }
            document.regions.push_back(std::move(record));
        }
        ValidateWorld(document); output=std::move(document);
    });
}
bool EncodeRegionContentDocument(const RegionContentDocument& document,std::string& output,std::string& error)
{
    return Guard(error,[&] { ValidateRegion(document);
        auto text=J{{"format","MaiX Region"},{"version",1},{"world",document.world},{"id",document.id},{"content",Content(document.content)}}.dump(2);
        Check(text.size()<=MaximumSceneDocumentBytes,"Region document exceeds 64 MiB."); output=std::move(text); });
}
bool DecodeRegionContentDocument(const std::string& text,RegionContentDocument& output,std::string& error)
{
    return Guard(error,[&] { const auto value=Parse(text);
        Check(value.is_object()&&value.size()==5&&value.at("format")=="MaiX Region"&&value.at("version").is_number_integer()&&value.at("version")==1,"Unsupported region format/version.");
        RegionContentDocument document; document.id=value.at("id").get<std::string>(); document.world=value.at("world").get<std::string>();
        document.content=Content(value.at("content")); ValidateRegion(document); output=std::move(document); });
}
bool ReadWorldContentDocument(const std::filesystem::path& file,WorldContentDocument& output,std::string& error)
{ return Guard(error,[&] { const auto text=Read(file); std::string message; Check(DecodeWorldContentDocument(text,output,message),message); }); }
bool ReadRegionContentDocument(const std::filesystem::path& file,RegionContentDocument& output,std::string& error)
{ return Guard(error,[&] { const auto text=Read(file); std::string message; Check(DecodeRegionContentDocument(text,output,message),message); }); }
bool WriteWorldContentDocument(const std::filesystem::path& file,const WorldContentDocument& document,std::string& error,bool overwrite)
{ std::string text; return EncodeWorldContentDocument(document,text,error)&&WriteTextFileAtomically(file,text,error,overwrite); }
bool WriteRegionContentDocument(const std::filesystem::path& file,const RegionContentDocument& document,std::string& error,bool overwrite)
{ std::string text; return EncodeRegionContentDocument(document,text,error)&&WriteTextFileAtomically(file,text,error,overwrite); }
