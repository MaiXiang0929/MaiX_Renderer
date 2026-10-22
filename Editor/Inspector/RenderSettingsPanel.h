// SPDX-License-Identifier: MIT
#pragma once

struct PostProcessSettings;

// Edits scene-owned values; no renderer or GPU resource ownership.
void DrawRenderSettingsPanel(PostProcessSettings& settings);
