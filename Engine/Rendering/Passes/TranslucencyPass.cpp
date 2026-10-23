// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Passes/TranslucencyPass.h"

#include "Engine/Rendering/Passes/ForwardPass.h"
#include "Engine/Rendering/Renderer/View/RenderView.h"

void TranslucencyPass::Execute(RenderPassContext& context)
{
    context.Resources().BeginTarget(PassResourceId::SceneHdrColor);
    RenderToBoundTarget(context, context.mainView);
    context.Resources().EndTarget(PassResourceId::SceneHdrColor);
    context.Resources().Publish(PassResourceId::SceneHdrColor);
    context.Resources().Publish(PassResourceId::SceneIndirect);
}

void TranslucencyPass::RenderToBoundTarget(
    RenderPassContext& context,
    RenderView& view)
{
    RHIScopedPipelineState stateScope(context.device.Commands());
    auto state=stateScope.saved; state.depthTest=true; state.depthWrite=false; state.blend=true;
    state.sourceRgb=state.sourceAlpha=RHIBlendFactor::SrcAlpha;
    state.destinationRgb=state.destinationAlpha=RHIBlendFactor::OneMinusSrcAlpha;
    context.device.Commands().SetPipelineState(state);

    m_ForwardPass.RenderTranslucentSurface(context, view);

}
