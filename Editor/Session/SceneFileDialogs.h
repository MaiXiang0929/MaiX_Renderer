// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <string>
// Drawn outside menus so a pending scene replacement survives menu closure.
void DrawSceneFileDialogs(bool& requestReplacement, bool replacementPending, std::string& error,
    const std::function<bool()>& save, const std::function<void()>& open,
    const std::function<void()>& cancel,const char* replacementTitle="Open another scene?");
