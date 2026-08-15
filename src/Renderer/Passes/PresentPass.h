// SPDX-License-Identifier: MIT
#pragma once

#include "Renderer/Pipeline/RenderPass.h"
#include "Renderer/Resources/Shader.h"

/// @brief Present Pass: copies display-ready color to the default framebuffer.
class PresentPass final : public RenderPass
{
public:
    bool Init();
    bool ReloadShaders();

    RenderPassType GetType() const override
    {
        return RenderPassType::Present;
    }
    void Execute(RenderPassContext& context) override;

private:
    Shader m_Shader;
};
