// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>

#include "Assets/Import/ImportedModelData.h"

namespace AssetImport
{
struct FbxImportOptions
{
    bool flipUv = true;
    bool generateMissingNormals = true;
};

class FbxModelImporter
{
public:
    static ModelImportResult Import(
        const std::filesystem::path& path,
        const FbxImportOptions& options = {});
};
}
