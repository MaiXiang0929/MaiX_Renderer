// SPDX-License-Identifier: MIT
#include "Engine/Core/Platform/Windows/FileDialog.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShObjIdl.h>
#include <wrl/client.h>

#include <cstddef>
#include <iterator>

namespace
{
using Microsoft::WRL::ComPtr;

class ScopedComInitialization
{
public:
    ScopedComInitialization()
        : m_Result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))
    {
    }

    ~ScopedComInitialization()
    {
        if (SUCCEEDED(m_Result))
            CoUninitialize();
    }

    bool IsAvailable() const
    {
        // RPC_E_CHANGED_MODE means COM is already initialized on this thread
        // with a different apartment model and is still available for use.
        return SUCCEEDED(m_Result) || m_Result == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT m_Result;
};

std::optional<std::filesystem::path> OpenFile(
    FileDialog::NativeWindowHandle ownerWindow,
    const wchar_t* title,
    const COMDLG_FILTERSPEC* filters,
    std::size_t filterCount,
    bool pickDirectory = false,
    const std::filesystem::path& initialDirectory = {}, bool saveFile = false)
{
    ScopedComInitialization comInitialization;
    if (!comInitialization.IsAvailable())
        return std::nullopt;

    ComPtr<IFileDialog> dialog;
    const HRESULT createResult = CoCreateInstance(
        saveFile ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(createResult))
        return std::nullopt;

    FILEOPENDIALOGOPTIONS options = {};
    if (FAILED(dialog->GetOptions(&options)))
        return std::nullopt;

    options |= FOS_FORCEFILESYSTEM |
               FOS_PATHMUSTEXIST |
               FOS_NOCHANGEDIR;
    options |= pickDirectory ? FOS_PICKFOLDERS : saveFile ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST;
    if (saveFile) { dialog->SetDefaultExtension(L"maixscene"); dialog->SetFileName(L"Scene.maixscene"); }

    if (FAILED(dialog->SetOptions(options)) ||
        (filterCount != 0 && FAILED(dialog->SetFileTypes(static_cast<UINT>(filterCount), filters))) ||
        (filterCount != 0 && FAILED(dialog->SetFileTypeIndex(1))) ||
        FAILED(dialog->SetTitle(title)))
    {
        return std::nullopt;
    }

    if (!initialDirectory.empty())
    {
        ComPtr<IShellItem> initial;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr,
                IID_PPV_ARGS(initial.GetAddressOf()))))
            dialog->SetFolder(initial.Get());
    }
    const HRESULT showResult = dialog->Show(static_cast<HWND>(ownerWindow));
    if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return std::nullopt;
    if (FAILED(showResult))
        return std::nullopt;

    ComPtr<IShellItem> selectedItem;
    if (FAILED(dialog->GetResult(selectedItem.GetAddressOf())))
        return std::nullopt;

    PWSTR selectedPath = nullptr;
    if (FAILED(selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)))
        return std::nullopt;

    const std::filesystem::path result(selectedPath);
    CoTaskMemFree(selectedPath);
    return result;
}
}

namespace FileDialog
{
std::optional<std::filesystem::path> OpenProjectFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory)
{
    static constexpr COMDLG_FILTERSPEC filters[]={{L"MaiX Project (*.maixproject)",L"*.maixproject"}};
    return OpenFile(ownerWindow,L"Open Project",filters,std::size(filters),false,initialDirectory);
}
std::optional<std::filesystem::path> OpenSceneFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory)
{
    static constexpr COMDLG_FILTERSPEC filters[]={{L"MaiX Scene (*.maixscene)",L"*.maixscene"}};
    return OpenFile(ownerWindow,L"Open Scene",filters,std::size(filters),false,initialDirectory);
}
std::optional<std::filesystem::path> SaveSceneFile(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory)
{
    static constexpr COMDLG_FILTERSPEC filters[]={{L"MaiX Scene (*.maixscene)",L"*.maixscene"}};
    return OpenFile(ownerWindow,L"Save Scene As",filters,std::size(filters),false,initialDirectory,true);
}
std::optional<std::filesystem::path> OpenDirectory(NativeWindowHandle ownerWindow,
    const std::filesystem::path& initialDirectory)
{
    return OpenFile(ownerWindow, L"Open Project Folder", nullptr, 0, true, initialDirectory);
}

std::optional<std::filesystem::path> OpenModelFile(NativeWindowHandle ownerWindow)
{
    static constexpr COMDLG_FILTERSPEC filters[] = {
        {L"Autodesk FBX (*.fbx)", L"*.fbx"},
    };

    return OpenFile(
        ownerWindow,
        L"Import Model",
        filters,
        std::size(filters));
}

std::optional<std::filesystem::path> OpenHdrEnvironmentFile(NativeWindowHandle ownerWindow)
{
    static constexpr COMDLG_FILTERSPEC filters[] = {{L"Radiance Environment (*.hdr)",L"*.hdr"}};
    return OpenFile(ownerWindow,L"Select HDR Environment",filters,std::size(filters));
}

std::optional<std::filesystem::path> OpenTextureFile(NativeWindowHandle ownerWindow)
{
    static constexpr COMDLG_FILTERSPEC filters[] = {
        {L"Supported Textures (*.png;*.jpg;*.jpeg)", L"*.png;*.jpg;*.jpeg"},
        {L"PNG Image (*.png)", L"*.png"},
        {L"JPEG Image (*.jpg;*.jpeg)", L"*.jpg;*.jpeg"},
    };

    return OpenFile(
        ownerWindow,
        L"Select Texture",
        filters,
        std::size(filters));
}
}
