// SPDX-License-Identifier: MIT
#include "Editor/Session/ProjectDocument.h"
#include "Engine/Core/Platform/Windows/AtomicFile.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <stdexcept>
namespace
{
namespace fs=std::filesystem;
void Check(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
bool Within(const fs::path& root,const fs::path& path)
{
    if(root.empty()) return false;
    auto p=path.begin();
    for(auto r=root.begin();r!=root.end();++r,++p)
    {
        if(p==path.end()) return false;
#ifdef _WIN32
        if(_wcsicmp(p->c_str(),r->c_str())!=0) return false;
#else
        if(*p!=*r) return false;
#endif
    }
    return true;
}
std::string Lower(std::string s) { for(auto& c:s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
void Name(const std::string& name)
{
    Check(!name.empty()&&name.size()<=128&&name.find_first_of("<>:\"/\\|?*\r\n\t")==std::string::npos&&name.find('\0')==std::string::npos,"Invalid project name.");
    Check(std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32;}),"Invalid control character in project name.");
    Check(name.back()!='.'&&name.back()!=' ',"Project name cannot end in a dot or space.");
    const auto stem=Lower(name.substr(0,name.find('.')));
    Check(stem!="con"&&stem!="prn"&&stem!="aux"&&stem!="nul"&&
        !(stem.size()==4&&(stem.substr(0,3)=="com"||stem.substr(0,3)=="lpt")&&stem[3]>='1'&&stem[3]<='9'),"Reserved project name.");
}
ProjectDocument Location(const fs::path& requested,const std::string& name,const std::vector<fs::path>& protectedDirectories)
{
    Check(requested.is_absolute(),"Choose an absolute project location.");
    ProjectDocument d; d.name=name; d.root=fs::weakly_canonical(requested); d.assets=fs::weakly_canonical(d.root/"Assets"); d.settingsRoot=d.root;
    for(const auto& p:protectedDirectories) if(!p.empty())
    {
        const auto root=fs::weakly_canonical(fs::absolute(p));
        Check(!Within(root,d.root)&&!Within(root,d.assets),"Choose a project outside engine resources and build directories.");
    }
    Check(Within(d.root,d.assets),"Assets resolves outside the project.");
    return d;
}
fs::path RelativeScene(const ProjectDocument& d)
{
    auto part=d.sceneFile.begin(); for(auto r=d.assets.begin();r!=d.assets.end();++r) ++part;
    fs::path result; for(;part!=d.sceneFile.end();++part) result/=*part;
    return result;
}
fs::path ScenePath(const ProjectDocument& d,const fs::path& requested,const std::vector<fs::path>& protectedDirectories)
{
    const auto path=fs::weakly_canonical(requested);
    Check(Within(d.assets,path),"Scene path resolves outside the project's Assets.");
    for(const auto& directory:protectedDirectories) if(!directory.empty())
        Check(!Within(fs::weakly_canonical(fs::absolute(directory)),path),"Scene path resolves inside a protected engine directory.");
    return path;
}
struct CreatedFiles
{
    std::vector<fs::path> files, directories;
    bool committed=false;
    ~CreatedFiles()
    {
        if(committed) return;
        std::error_code ignored;
        for(auto it=files.rbegin();it!=files.rend();++it) fs::remove(*it,ignored);
        // Non-recursive: preserve anything created by another writer.
        for(auto it=directories.rbegin();it!=directories.rend();++it) fs::remove(*it,ignored);
    }
    void Directory(const fs::path& p) { if(fs::create_directory(p)) directories.push_back(p); }
};
bool Initialize(const fs::path& root,const std::string& name,const fs::path& existing,
    const SceneDocument& initial,const std::vector<fs::path>& protectedDirectories,
    ProjectDocument& output,std::string& error,bool newProject)
{
    CreatedFiles created;
    try
    {
        Name(name); auto d=Location(root,name,protectedDirectories);
        Check(newProject?!fs::exists(d.root):fs::is_directory(d.root),newProject?"Project folder already exists; choose another name.":"Project folder does not exist.");
        if(newProject) created.Directory(d.root);
        Check(FindProjectDocument(d.root).empty(),"This directory already contains a project descriptor.");
        d.file=d.root/fs::u8path(name+".maixproject");
        created.Directory(d.assets);
        if(existing.empty())
        {
            const auto directory=ScenePath(d,d.assets/"Scenes",protectedDirectories);
            d.sceneFile=ScenePath(d,directory/"Main.maixscene",protectedDirectories);
            created.Directory(directory);
            Check(!fs::exists(d.sceneFile),"Main.maixscene already exists. Select the existing scene instead.");
            if(!WriteSceneDocument(d.sceneFile,initial,error,false)) throw std::runtime_error(error);
            created.files.push_back(d.sceneFile);
        }
        else
        {
            Check(existing.is_absolute(),"Select an absolute scene path.");
            d.sceneFile=ScenePath(d,existing,protectedDirectories);
            Check(Within(d.assets,d.sceneFile)&&Lower(d.sceneFile.extension().string())==".maixscene","Default scene must be inside this project's Assets.");
            SceneDocument scene; if(!ReadSceneDocument(d.sceneFile,scene,error)) throw std::runtime_error(error);
        }
        const nlohmann::json value={{"format","MaiX Project"},{"version",1},{"name",name},{"startScene",RelativeScene(d).generic_u8string()}};
        if(!WriteTextFileAtomically(d.file,value.dump(2),error,false)) throw std::runtime_error(error);
        created.files.push_back(d.file);
        output=std::move(d); created.committed=true; error.clear(); return true;
    }
    catch(const std::exception& e) { error=e.what(); return false; }
}
}
std::filesystem::path FindProjectDocument(const std::filesystem::path& root)
{
    if(!fs::is_directory(root)) return {};
    fs::path result;
    for(const auto& entry:fs::directory_iterator(root))
        if(entry.is_regular_file()&&Lower(entry.path().extension().string())==".maixproject")
        { Check(result.empty(),"Multiple project descriptors found. Open a .maixproject file explicitly."); result=entry.path(); }
    return result;
}
bool ReadProjectDocument(const fs::path& file,const std::vector<fs::path>& protectedDirectories,ProjectDocument& output,std::string& error)
{
    try
    {
        Check(file.is_absolute()&&Lower(file.extension().string())==".maixproject","Choose an absolute .maixproject path.");
        std::ifstream input(file,std::ios::binary|std::ios::ate); Check(static_cast<bool>(input),"Cannot open project descriptor.");
        const auto size=input.tellg(); Check(size>0&&size<=65536,"Invalid project descriptor size.");
        std::string text(static_cast<size_t>(size),'\0'); input.seekg(0);
        Check(static_cast<bool>(input.read(text.data(),static_cast<std::streamsize>(size))),"Cannot read project descriptor.");
        std::unordered_set<std::string> keys;
        const auto j=nlohmann::json::parse(text,[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& value){
            Check(depth<=1,"Unexpected nested project data.");
            if(event==nlohmann::json::parse_event_t::key) Check(keys.insert(value.get<std::string>()).second,"Duplicate project field.");
            return true;
        });
        Check(j.is_object()&&j.size()==4&&j.at("format")=="MaiX Project"&&j.at("version").is_number_integer()&&j.at("version")==1,"Unsupported project format/version.");
        const auto name=j.at("name").get<std::string>(); Name(name);
        auto d=Location(fs::weakly_canonical(file).parent_path(),name,protectedDirectories);
        d.file=fs::weakly_canonical(file);
        const auto relative=j.at("startScene").get<std::string>();
        d.sceneFile=ScenePath(d,ResolveSceneAssetPath({"project",relative},{d.assets,{},{}}),protectedDirectories);
        Check(Lower(d.sceneFile.extension().string())==".maixscene"&&fs::is_regular_file(d.sceneFile),"Project default scene is missing or invalid.");
        Check(fs::is_directory(d.assets),"Project Assets directory is missing.");
        output=std::move(d); error.clear(); return true;
    }
    catch(const std::exception& e) { error=e.what(); return false; }
}
bool CreateProjectDocument(const fs::path& parent,const std::string& name,const SceneDocument& initial,
    const std::vector<fs::path>& protectedDirectories,ProjectDocument& output,std::string& error)
{
    try { Name(name); Check(parent.is_absolute()&&fs::is_directory(parent),"Select an existing parent directory."); }
    catch(const std::exception& e) { error=e.what(); return false; }
    return Initialize(parent/fs::u8path(name),name,{},initial,protectedDirectories,output,error,true);
}
bool InitializeLegacyProject(const fs::path& root,const fs::path& existing,const SceneDocument& initial,
    const std::vector<fs::path>& protectedDirectories,ProjectDocument& output,std::string& error)
{
    try
    {
        const auto canonical=fs::weakly_canonical(root);
        // Selecting the content root of a known project must not create Assets/Assets.
        if(Lower(canonical.filename().u8string())=="assets"&&!FindProjectDocument(canonical.parent_path()).empty())
        {
            error="This is a project's Assets folder. Open the project in: "+canonical.parent_path().u8string();
            return false;
        }
    }
    catch(const std::exception& e) { error=e.what(); return false; }
    return Initialize(root,root.filename().u8string(),existing,initial,protectedDirectories,output,error,false);
}
