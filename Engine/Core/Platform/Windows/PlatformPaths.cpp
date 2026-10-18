// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/Windows/PlatformPaths.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShlObj.h>
#include <string>

std::filesystem::path PlatformPaths::DocumentsDirectory()
{
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &path)))
        return {};
    const std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

std::filesystem::path PlatformPaths::ExecutableDirectory()
{
    std::wstring buffer(512, L'\0');
    while (buffer.size() <= 32768)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size())
        {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
    return {};
}
