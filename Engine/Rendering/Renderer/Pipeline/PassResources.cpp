// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Pipeline/PassResourceContract.h"
void PassResources::BeginLayer(PassResourceId id, unsigned layer)
{
    const auto& binding = Output(id);
    if ((id != PassResourceId::ShadowDepth && id != PassResourceId::ShadowCubeDepth) ||
        layer >= binding.requiredLayers || m_ActiveLayerResource != PassResourceId::Count)
        Fail(id, "invalid shadow layer write.");
    Commands().BeginTargetLayer(binding.framebuffer, layer, true);
    m_ActiveLayerResource = id;
    m_ActiveLayer = layer;
}
void PassResources::EndLayer(PassResourceId id)
{
    if (id != m_ActiveLayerResource) Fail(id, "shadow layer write was not begun.");
    Commands().EndTarget();
    m_Frame.m_WrittenLayers[static_cast<std::size_t>(id)][m_ActiveLayer] = true;
    m_ActiveLayerResource = PassResourceId::Count;
}
RHICommandList &PassResources::Commands() const
{
    if (!m_Commands)
        throw ResourceContractError("No RHI commands attached to pass resources");
    return *m_Commands;
}
void PassResources::BeginTarget(PassResourceId id) const
{
    const auto &binding = Output(id);
    const auto &declaration = FindOutput(id);
    Commands().BeginTarget(binding.framebuffer, declaration.colorLoad == AttachmentLoad::Clear,
                           declaration.depthLoad == AttachmentLoad::Clear,
                           id == PassResourceId::Overlay ? 0.0f : 1.0f);
}
void PassResources::EndTarget(PassResourceId id, bool generateMipmaps) const
{
    const auto &binding = Output(id);
    Commands().EndTarget();
    if (generateMipmaps && binding.mipmaps)
        Commands().GenerateMipmaps(binding.texture);
}
