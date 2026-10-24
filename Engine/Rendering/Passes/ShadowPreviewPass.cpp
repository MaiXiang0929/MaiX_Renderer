// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/ShadowPreviewPass.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include <algorithm>
bool ShadowPreviewPass::Prepare()
{
    FramebufferSpecification spec;
    spec.width = spec.height = 256;
    spec.depthStencilEnabled = spec.mipmapsEnabled = false;
    return m_Target.Init(spec);
}
void ShadowPreviewPass::Execute(RenderPassContext& context)
{
    if (!context.frame.shadowsEnabled || context.frame.shadowPreviewLight < 0 ||
        context.frame.shadowPreviewLight >= static_cast<int>(context.frame.shadowFrame.lights.size())) return;
    const auto& light = context.frame.shadowFrame.lights[context.frame.shadowPreviewLight];
    const bool point = light.type == LightType::Point;
    const unsigned slice = std::min(static_cast<unsigned>(std::max(0, context.frame.shadowPreviewSlice)),
        point ? 5u : light.cascadeCount - 1);
    const int base = context.frame.shadowPreviewReflection && light.reflectionBaseLayer >= 0
        ? light.reflectionBaseLayer : light.baseLayer;
    context.Resources().BeginTarget(PassResourceId::ShadowPreview);
    RHIScopedPipelineState scope(m_Device.Commands());
    auto state = scope.saved; state.depthTest = state.depthWrite = false;
    m_Device.Commands().SetPipelineState(state);
    m_Shader.Bind();
    m_Device.Commands().BindTexture(context.Resources().Texture(PassResourceId::ShadowDepth),0,RHITextureKind::Texture2DArray);
    m_Device.Commands().BindTexture(context.Resources().Texture(PassResourceId::ShadowCubeDepth),1,RHITextureKind::CubemapArray);
    m_Shader.SetInt("depthArray",0); m_Shader.SetInt("depthCubes",1);
    m_Shader.SetInt("point",point); m_Shader.SetInt("slice",slice);
    m_Shader.SetInt("layer",point ? light.cubeIndex : std::max(0,base) + slice);
    context.presentMesh.Draw();
    context.Resources().EndTarget(PassResourceId::ShadowPreview,false);
    context.Resources().Publish(PassResourceId::ShadowPreview);
}
