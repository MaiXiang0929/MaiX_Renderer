// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Shader.h"

class ForwardPass;

/// Draws artist-controlled inverted hulls into the Forward HDR target.
class OutlinePass final : public RenderPass
{
public:
    explicit OutlinePass(ForwardPass& forwardPass)
        : m_ForwardPass(forwardPass)
    {
    }

    bool Init();
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::Outline;
    }
    void Execute(RenderPassContext& context) override;

private:
    ForwardPass& m_ForwardPass;
    Shader m_Shader;
};
