// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/Windows/AtomicFile.h"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <system_error>

bool WriteTextFileAtomically(const std::filesystem::path& path,const std::string& text,std::string& error,bool overwrite)
{
    static std::atomic<unsigned long long> sequence{0};
    auto temporary=path;
    temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++sequence);
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE)
    { error="Cannot create temporary scene file: "+std::system_category().message(GetLastError()); return false; }
    bool success=true;
    DWORD failure=ERROR_SUCCESS;
    size_t offset=0;
    while (offset<text.size())
    {
        const DWORD bytes=static_cast<DWORD>(std::min<size_t>(text.size()-offset,1024*1024));
        DWORD written=0;
        if (!WriteFile(file,text.data()+offset,bytes,&written,nullptr)||written!=bytes)
        { success=false; failure=GetLastError(); break; }
        offset+=written;
    }
    if (success&&!FlushFileBuffers(file)) { success=false; failure=GetLastError(); }
    CloseHandle(file);
    if (success&&!MoveFileExW(temporary.c_str(),path.c_str(),(overwrite?MOVEFILE_REPLACE_EXISTING:0)|MOVEFILE_WRITE_THROUGH))
    { success=false; failure=GetLastError(); }
    if (!success)
    {
        DeleteFileW(temporary.c_str());
        error="Scene write failed: "+std::system_category().message(failure); return false;
    }
    error.clear(); return true;
}
