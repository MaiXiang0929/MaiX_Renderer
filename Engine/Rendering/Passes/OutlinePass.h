// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/Rendering/Renderer/Pipeline/RenderPass.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"

/// Draws artist-controlled inverted hulls into the Forward HDR target.
class OutlinePass final : public RenderPass
{
public:
    explicit OutlinePass(RHIDevice& device) : m_Device(device), m_Shader(device) {}
    bool Init();
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::Outline;
    }
    void Execute(RenderPassContext& context) override;

private:
    RHIDevice& m_Device;
    Shader m_Shader;
};
