// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/SceneDocument.h"
#include "Engine/Core/Platform/Windows/AtomicFile.h"
#include <fstream>

bool ReadSceneDocument(const std::filesystem::path& path,SceneDocument& output,std::string& error)
{
    try
    {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if (!input) { error="Cannot open scene: "+path.u8string(); return false; }
        const auto size=input.tellg();
        if (size<0||size>static_cast<std::streamoff>(MaximumSceneDocumentBytes)) { error="Invalid scene size (maximum 64 MiB)."; return false; }
        std::string text(static_cast<size_t>(size),'\0'); input.seekg(0);
        if (!text.empty()&&!input.read(text.data(),static_cast<std::streamsize>(text.size())))
        { error="Cannot read complete scene file."; return false; }
        return DecodeSceneDocument(text,output,error);
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}
bool WriteSceneDocument(const std::filesystem::path& path,const SceneDocument& d,std::string& error,bool overwrite)
{
    try
    {
        std::string text;
        if (!EncodeSceneDocument(d,text,error)) return false;
        return WriteTextFileAtomically(path,text,error,overwrite);
    }
    catch (const std::exception& e) { error=e.what(); return false; }
}
