// SPDX-License-Identifier: MIT
#include "Platform/Windows/FileDialog.h"

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
    std::size_t filterCount)
{
    ScopedComInitialization comInitialization;
    if (!comInitialization.IsAvailable())
        return std::nullopt;

    ComPtr<IFileOpenDialog> dialog;
    const HRESULT createResult = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(createResult))
        return std::nullopt;

    FILEOPENDIALOGOPTIONS options = {};
    if (FAILED(dialog->GetOptions(&options)))
        return std::nullopt;

    options |= FOS_FORCEFILESYSTEM |
               FOS_FILEMUSTEXIST |
               FOS_PATHMUSTEXIST |
               FOS_NOCHANGEDIR;

    if (FAILED(dialog->SetOptions(options)) ||
        FAILED(dialog->SetFileTypes(static_cast<UINT>(filterCount), filters)) ||
        FAILED(dialog->SetFileTypeIndex(1)) ||
        FAILED(dialog->SetTitle(title)))
    {
        return std::nullopt;
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
