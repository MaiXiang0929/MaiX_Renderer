// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
struct PostProcessSettings;

// Edits scene-owned values; no renderer or GPU resource ownership.
void DrawRenderSettingsPanel(PostProcessSettings& settings, const std::function<void()>& environmentControls = {});
