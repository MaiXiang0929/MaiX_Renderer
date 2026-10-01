// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>
#include <array>
#include <functional>
#include <string>

#include "Renderer/Pipeline/RenderPass.h"

namespace
{
void Require(bool condition, const char* message)
{
    if (condition)
        return;
    std::cerr << "[RenderPassContractTests] " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

using Id = PassResourceId;
constexpr std::size_t PassCount = static_cast<std::size_t>(RenderPassType::Count);
using Contracts = std::array<PassResourceContract, PassCount>;

Contracts MakeContracts(PassResourceFeatures features)
{
    Contracts contracts;
    for (std::size_t i = 0; i < PassCount; ++i)
        contracts[i] = BuildPassResourceContract(static_cast<RenderPassType>(i), features);
    return contracts;
}

// 使用无 GL 上下文的资源快照验证契约；主颜色三个阶段明确共享纹理 4 / FBO 104。
void BindFixture(FrameResources& frame, unsigned int width = 1281, unsigned int height = 721)
{
    frame.BeginFrame(width, height);
    frame.Bind(Id::Environment, {0, 0, 0, 0, 0, ResourceFormat::Cubemap, false, true});
    frame.Bind(Id::ShadowDepth, {2, 102, 2, 2048, 2048, ResourceFormat::Depth24, true});
    frame.Bind(Id::ReflectionColor, {3, 103, 0, (width + 1) / 2, (height + 1) / 2, ResourceFormat::RGBA8, true});
    for (Id id : {Id::ForwardColor, Id::OutlinedColor, Id::SceneHdrColor})
        frame.Bind(id, {4, 104, 5, width, height, ResourceFormat::RGBA16F, true});
    frame.Bind(Id::ForwardDepth, {5, 104, 5, width, height, ResourceFormat::Depth24Stencil8, true});
    frame.Bind(Id::SsaoAO, {6, 106, 0, (width + 1) / 2, (height + 1) / 2, ResourceFormat::R8});
    frame.Bind(Id::SsaoColor, {7, 107, 0, width, height, ResourceFormat::RGBA16F});
    frame.Bind(Id::Bloom, {8, 108, 0, (width + 1) / 2, (height + 1) / 2, ResourceFormat::RGBA16F, true});
    frame.Bind(Id::PostColor, {9, 109, 0, width, height, ResourceFormat::RGBA8, true});
    frame.Bind(Id::Overlay, {10, 110, 0, width, height, ResourceFormat::RGBA8});
    frame.Bind(Id::FinalColor, {11, 111, 0, width, height, ResourceFormat::RGBA8});
}

void Submit(FrameResources& frame, const PassResourceContract& contract)
{
    PassResources access(frame, contract);
    access.Validate();
    for (std::size_t i = 0; i < contract.outputCount; ++i)
        access.Publish(contract.outputs[i].id);
    access.Complete();
}
void SubmitBefore(FrameResources& frame, const Contracts& contracts, RenderPassType pass)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(pass); ++i)
        Submit(frame, contracts[i]);
}
void ExpectFailure(const std::function<void()>& operation, const char* diagnostic)
{
    try { operation(); }
    catch (const ResourceContractError& error)
    {
        Require(std::string(error.what()).find(diagnostic) != std::string::npos,
            "Contract failure must identify the expected reason or resource.");
        return;
    }
    Require(false, "Invalid resource access unexpectedly succeeded.");
}

void TestFeatureRoutingAndFrameReset()
{
    FrameResources frame;
    for (unsigned int flags = 0; flags < 8; ++flags)
    {
        const bool shadows = (flags & 1) != 0;
        const bool ssao = (flags & 2) != 0;
        const bool bloom = (flags & 4) != 0;
        const auto contracts = MakeContracts({shadows, ssao, bloom});
        std::string error;
        Require(ValidatePassResourceSequence(contracts.data(), contracts.size(), error),
            "Every supported feature combination needs a complete fixed resource sequence.");
        BindFixture(frame);
        Require(frame.GetFinalTexture() == 0, "A new frame must not expose allocated final color.");
        for (const auto& contract : contracts) Submit(frame, contract);
        Require(frame.GetFinalTexture() == 11, "Only completed Present can expose final color.");
        Require(frame.IsProduced(Id::ShadowDepth) == shadows, "Disabled shadows must not publish old depth.");
        Require(frame.IsProduced(Id::SsaoColor) == ssao, "Disabled SSAO must not publish old composite.");
        Require(frame.IsProduced(Id::Bloom) == bloom, "Disabled Bloom must not publish old highlights.");
        const auto& post = contracts[static_cast<std::size_t>(RenderPassType::PostProcess)];
        PassResources access(frame, post);
        Require(access.HdrTexture() == (ssao ? 7u : 4u), "HDR input must follow explicit SSAO routing.");
    }
    frame.BeginFrame(0, 0);
    Require(frame.GetFinalTexture() == 0, "A minimized/failed next frame must not expose stale output.");
}

void TestSequenceErrors()
{
    auto contracts = MakeContracts({true, false, true});
    std::string error;
    std::swap(contracts[4], contracts[6]);
    Require(!ValidatePassResourceSequence(contracts.data(), contracts.size(), error) &&
        error.find("SceneHdrColor") != std::string::npos,
        "Bloom before transparency must identify the unfinished SceneHdrColor stage.");
    contracts = MakeContracts({true, false, true});
    contracts[0].outputCount = 0;
    Require(!ValidatePassResourceSequence(contracts.data(), contracts.size(), error) &&
        error.find("ShadowDepth") != std::string::npos, "Missing shadow producer must be detected.");
    contracts = MakeContracts({false, false, false});
    contracts[3].inputs[0] = {Id::Environment, ResourceAccess::Sample, true};
    Require(!ValidatePassResourceSequence(contracts.data(), contracts.size(), error) &&
        error.find("preserved source") != std::string::npos,
        "Attachment preservation needs an explicit matching input.");
}

void TestAccessAndPublication()
{
    const auto contracts = MakeContracts({false, false, false});
    FrameResources frame;
    BindFixture(frame);
    const auto& forward = contracts[2];
    PassResources access(frame, forward);
    ExpectFailure([&] { access.Validate(); }, "ReflectionColor");
    ExpectFailure([&] { access.Texture(Id::Bloom); }, "not declared");
    ExpectFailure([&] { access.Publish(Id::FinalColor); }, "not declared");
    ExpectFailure([&] { access.Complete(); }, "not published");
    SubmitBefore(frame, contracts, RenderPassType::Outline);
    Submit(frame, contracts[3]); // 模拟没有描边对象的显式保留发布。
    Require(!frame.IsProduced(Id::ForwardColor) && frame.IsProduced(Id::OutlinedColor),
        "Reusing a target must invalidate its previous content identity.");
    Submit(frame, contracts[4]); // 模拟空透明队列。
    Require(!frame.IsProduced(Id::OutlinedColor) && frame.IsProduced(Id::SceneHdrColor),
        "Empty transparency must still finish the HDR content stage.");
    PassResources disabled(frame, contracts[6]);
    ExpectFailure([&] { disabled.Output(Id::Bloom); }, "not declared");
    auto oldRead = contracts[6];
    oldRead.inputs[0] = {Id::ForwardColor, ResourceAccess::Sample};
    oldRead.inputCount = 1;
    PassResources stale(frame, oldRead);
    ExpectFailure([&] { stale.Validate(); }, "not been produced");
}

void TestBindingsAndFeedback()
{
    const auto contracts = MakeContracts({false, true, true});
    FrameResources frame;
    BindFixture(frame);
    SubmitBefore(frame, contracts, RenderPassType::Bloom);
    PassResources bloom(frame, contracts[6]);
    frame.Bind(Id::Bloom, {8, 108, 0, 640, 360, ResourceFormat::RGBA16F});
    ExpectFailure([&] { bloom.Validate(); }, "extent");
    frame.Bind(Id::Bloom, {0, 108, 0, 641, 361, ResourceFormat::RGBA16F});
    ExpectFailure([&] { bloom.Validate(); }, "zero texture");
    frame.Bind(Id::Bloom, {8, 108, 0, 641, 361, ResourceFormat::RGBA8});
    ExpectFailure([&] { bloom.Validate(); }, "format");
    frame.Bind(Id::Bloom, {7, 108, 0, 641, 361, ResourceFormat::RGBA16F});
    ExpectFailure([&] { bloom.Validate(); }, "overlapping");

    BindFixture(frame);
    SubmitBefore(frame, contracts, RenderPassType::Outline);
    frame.Bind(Id::OutlinedColor, {4, 999, 5, 1281, 721, ResourceFormat::RGBA16F, true});
    PassResources outline(frame, contracts[3]);
    ExpectFailure([&] { outline.Validate(); }, "preserve");

    BindFixture(frame);
    SubmitBefore(frame, contracts, RenderPassType::Forward);
    frame.Bind(Id::ForwardColor, {4, 104, 0, 1281, 721, ResourceFormat::RGBA16F, false});
    PassResources forward(frame, contracts[2]);
    ExpectFailure([&] { forward.Validate(); }, "depth attachment");
    ExpectFailure([&] { frame.Bind(Id::SceneHdrColor,
        {4, 104, 5, 1281, 721, ResourceFormat::RGBA16F, true, true}); }, "imported");
}
}

int main()
{
    Require(static_cast<std::size_t>(RenderPassType::Shadow) == 0 &&
        static_cast<std::size_t>(RenderPassType::Forward) == 2 &&
        static_cast<std::size_t>(RenderPassType::Outline) == 3 &&
        static_cast<std::size_t>(RenderPassType::Translucency) == 4 &&
        static_cast<std::size_t>(RenderPassType::SSAO) == 5 &&
        static_cast<std::size_t>(RenderPassType::Present) == 9,
        "Pass enum order must match pipeline diagnostics ordering.");
    Require(static_cast<std::size_t>(RenderPassType::Count) == 10,
        "Pass diagnostics must reserve one slot per executable pass.");

    TestFeatureRoutingAndFrameReset();
    TestSequenceErrors();
    TestAccessAndPublication();
    TestBindingsAndFeedback();

    std::cout << "Render pass contract tests passed." << std::endl;
    return EXIT_SUCCESS;
}
