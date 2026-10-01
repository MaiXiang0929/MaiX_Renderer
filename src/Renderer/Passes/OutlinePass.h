// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Shader.h"

/// Draws artist-controlled inverted hulls into the Forward HDR target.
class OutlinePass final : public RenderPass
{
public:
    bool Init();
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::Outline;
    }
    void Execute(RenderPassContext& context) override;

private:
    Shader m_Shader;
};
