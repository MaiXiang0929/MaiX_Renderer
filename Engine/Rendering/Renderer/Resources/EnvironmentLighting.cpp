// SPDX-License-Identifier: MIT
#include "Engine/Rendering/Renderer/Resources/EnvironmentLighting.h"
#include "Engine/Rendering/Renderer/Resources/Shader.h"
#include "Engine/Rendering/Renderer/Resources/Mesh.h"
#include "Engine/Rendering/Renderer/Resources/Framebuffer.h"
#include <iostream>
#include <utility>

EnvironmentLighting::~EnvironmentLighting()
{
    m_Device.DestroyTexture(m_Irradiance);
    m_Device.DestroyTexture(m_Prefiltered);
    m_Device.DestroyTarget(m_Brdf.handle);
}

bool EnvironmentLighting::Build(RHITextureHandle environment, unsigned sourceResolution)
{
    if (!environment || !sourceResolution) return false;
    auto& commands = m_Device.Commands();
    RHIScopedPipelineState stateScope(commands);
    struct EndBake
    {
        RHICommandList& commands;
        ~EndBake() { commands.EndTarget(); commands.BindProgram(0); commands.InvalidateState(); }
    } endBake{commands};
    try
    {
        EnvironmentLighting candidate(m_Device);
        Shader bake(m_Device);
        if (!bake.Load("Shaders/postprocess/fullscreen.vert", "Shaders/environment/ibl_bake.frag")) return false;
        Mesh quad(m_Device);
        std::vector<Vertex> vertices(6);
        const float corners[6][2] = {{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for (unsigned i=0; i<6; ++i)
        {
            vertices[i].Position = {corners[i][0],corners[i][1],0};
            vertices[i].TexCoord = {(corners[i][0]+1)*0.5f,(corners[i][1]+1)*0.5f};
        }
        quad.Upload(vertices);
        RHITextureDesc texture;
        texture.width=texture.height=IrradianceSize;
        texture.kind=RHITextureKind::Cubemap; texture.format=RHITextureFormat::RGBA16F;
        texture.wrap=RHIWrap::ClampEdge;
        candidate.m_Irradiance=m_Device.CreateTexture(texture);
        texture.width=texture.height=PrefilterSize; texture.mipmaps=true;
        candidate.m_Prefiltered=m_Device.CreateTexture(texture);
        RHITargetDesc lut; lut.width=lut.height=BrdfSize;
        lut.depth=false; lut.colorFormat=RHITextureFormat::RGBA16F;
        candidate.m_Brdf=m_Device.CreateTarget(lut);
        if (!candidate.m_Irradiance || !candidate.m_Prefiltered || !candidate.m_Brdf.handle) return false;
        RHIPipelineState state; state.depthTest=false; state.depthWrite=false;
        commands.SetPipelineState(state);
        bake.Bind(); bake.SetInt("environmentMap",0);
        bake.SetFloat("sourceResolution",static_cast<float>(sourceResolution));
        commands.BindTexture(environment,0,RHITextureKind::Cubemap);
        Framebuffer scratch(m_Device);
        FramebufferSpecification spec; spec.colorFormat=FramebufferColorFormat::RGBA16F;
        spec.depthStencilEnabled=false; spec.mipmapsEnabled=false;
        for (int mode=0; mode<2; ++mode)
        {
            const unsigned maxMip = mode ? PrefilterMaxMip : 0;
            bake.SetInt("mode",mode);
            for (unsigned mip=0; mip<=maxMip; ++mip)
            {
                spec.width=spec.height=(mode ? PrefilterSize : IrradianceSize)>>mip;
                if (!scratch.Init(spec)) return false;
                bake.SetFloat("roughness",mode ? float(mip)/float(maxMip) : 0.0f);
                for (unsigned face=0; face<6; ++face)
                {
                    scratch.Bind(); bake.SetInt("face",static_cast<int>(face)); quad.Draw();
                    commands.CopyTargetToCubemap(scratch.GetFramebufferId(),
                        mode ? candidate.m_Prefiltered : candidate.m_Irradiance,face,mip);
                }
                commands.EndTarget();
            }
        }
        commands.BeginTarget(candidate.m_Brdf.handle,true,false,1);
        bake.SetInt("mode",2); quad.Draw(); commands.EndTarget();
        commands.BindTexture(0,0,RHITextureKind::Cubemap);
        std::swap(m_Irradiance,candidate.m_Irradiance);
        std::swap(m_Prefiltered,candidate.m_Prefiltered);
        std::swap(m_Brdf,candidate.m_Brdf);
        return true;
    }
    catch (const RHIError& error)
    {
        std::cerr << "[IBL] " << error.what() << '\n';
        return false;
    }
}
