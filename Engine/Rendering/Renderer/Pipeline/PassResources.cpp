// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Pipeline/PassResourceContract.h"
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
