// SPDX-License-Identifier: MIT
#pragma once

enum class RenderPassType
{
    Shadow, Reflection, Forward, Outline, Translucency, SSAO,
    Bloom, PostProcess, EditorPrimitive, Present, Count
};

inline const char* RenderPassName(RenderPassType type)
{
    static constexpr const char* names[] = {
        "Shadow", "Reflection", "Forward", "Outline", "Translucency",
        "SSAO", "Bloom", "PostProcess", "EditorPrimitive", "Present"};
    return type >= RenderPassType::Shadow && type < RenderPassType::Count
        ? names[static_cast<unsigned int>(type)] : "Unknown";
}
