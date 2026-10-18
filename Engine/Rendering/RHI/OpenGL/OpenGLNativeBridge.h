// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Rendering/RHI/RHIDevice.h"
// Backend escape hatch for ImGui and real-GL validation only. Never owns objects.
// Do not retain IDs across resource destruction, resize, or shader reload.
unsigned int GetOpenGLTexture(RHIDevice &, RHITextureHandle);
unsigned int GetOpenGLTarget(RHIDevice &, RHITargetHandle);
unsigned int GetOpenGLProgram(RHIDevice &, RHIProgramHandle);
