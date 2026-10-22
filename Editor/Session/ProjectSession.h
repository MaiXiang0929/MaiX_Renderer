// SPDX-License-Identifier: MIT
#pragma once
#include "Editor/Session/EditorProject.h"
struct SceneState;
class Renderer;
bool OpenProjectSession(EditorProject&,SceneState&,Renderer&,const SceneAssetRoots& engineRoots,
    const std::filesystem::path& file,std::string& error);
