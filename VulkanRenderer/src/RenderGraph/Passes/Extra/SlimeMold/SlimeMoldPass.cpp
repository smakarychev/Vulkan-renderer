#include "SlimeMoldPass.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Core/Random.h"
#include "imgui/imgui.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "Vulkan/RenderCommand.h"

SlimeMoldContext SlimeMoldContext::RandomIn(const glm::uvec2& bounds, u32 traitCount, u32 slimeCount,
    ResourceUploader& resourceUploader)
{
    ASSERT(traitCount > 0, "Simulation must include at least one trait")
    
    SlimeMoldContext ctx = {};
    ctx.m_Resolution = bounds;
    
    ctx.m_Traits.resize(traitCount);
    for (auto& trait : ctx.m_Traits)
        trait = RandomTrait();

    ctx.m_Slime.resize(slimeCount);
    for (auto& slime : ctx.m_Slime)
        slime = {
            .Position = {Random::Float(0.0f, (f32)bounds.x), Random::Float(0.0f, (f32)bounds.y)},
            .Angle = Random::Float(0.0f, glm::radians(360.0f)),
            .TraitsIndex = Random::UInt32(0, traitCount - 1),
            .ContagionStepsLeft = 0};

    ctx.m_TraitsBuffer = Buffer::Builder({
            .SizeBytes = (u32)ctx.m_Traits.size() * sizeof(Traits),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress | BufferUsage::Destination})
        .Build();

    ctx.m_SlimeBuffer = Buffer::Builder({
            .SizeBytes = (u32)ctx.m_Slime.size() * sizeof(Slime),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress | BufferUsage::Destination})
        .Build();

    ctx.m_SlimeMap = Texture::Builder({
            .Width = bounds.x,
            .Height = bounds.y,
            .Format = Format::RGBA16_FLOAT,
            .Usage = ImageUsage::Storage | ImageUsage::Destination})
        .Build();

    resourceUploader.UpdateBuffer(ctx.m_TraitsBuffer, ctx.m_Traits.data(),
        ctx.m_TraitsBuffer.GetSizeBytes(), 0);
    resourceUploader.UpdateBuffer(ctx.m_SlimeBuffer, ctx.m_Slime.data(),
        ctx.m_SlimeBuffer.GetSizeBytes(), 0);

    return ctx;
}

SlimeMoldContext::Traits SlimeMoldContext::RandomTrait()
{
    return {
        .MovementSpeed = Random::Float(500.0f, 3000.0f),
        .TurningSpeed = Random::Float(5.0f, 50.0f),
        .SensorAngle = Random::Float(0.0f, glm::radians(180.0f)),
        .SensorOffset = Random::Float(1.0f, 50.0f),
        .Color = glm::vec4(1.0f),
        .ContagionThreshold = Random::Float(0, 1e-1f),
        .ContagionSteps = Random::UInt32(1, 25)};
}

void SlimeMoldContext::UpdateTraits(ResourceUploader& resourceUploader)
{
    resourceUploader.UpdateBuffer(m_TraitsBuffer, m_Traits.data(), m_TraitsBuffer.GetSizeBytes(), 0);
}

SlimeMoldPass::SlimeMoldPass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* slimeTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
            "../assets/shaders/processed/render-graph/extra/slime-mold/slime-comp.stage"},
        "Pass.SlimeMold", renderGraph.GetArenaAllocators());

    m_UpdateSlimeMapPipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(slimeTemplate)
        .AddSpecialization("SLIME_MAP_STAGE", true)
        .UseDescriptorBuffer()
        .Build();

    m_UpdateSlimeMapPipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(slimeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    m_DiffuseSlimeMapPipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(slimeTemplate)
        .AddSpecialization("SLIME_DIFFUSE_STAGE", true)
        .UseDescriptorBuffer()
        .Build();

    m_DiffuseSlimeMapPipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(slimeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    m_GradientSlimeMapPipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(slimeTemplate)
        .AddSpecialization("SLIME_GRADIENT_STAGE", true)
        .UseDescriptorBuffer()
        .Build();

    m_GradientSlimeMapPipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(slimeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    m_CopyDiffuseToMapPass = std::make_shared<CopyTexturePass>("Copy.SlimeDiffuse");
}

void SlimeMoldPass::AddToGraph(RG::Graph& renderGraph, SlimeMoldPassStage stage, SlimeMoldContext& ctx)
{
    switch (stage)
    {
    case SlimeMoldPassStage::UpdateSlimeMap:
        AddUpdateSlimeMapStage(renderGraph, ctx);
        break;
    case SlimeMoldPassStage::DiffuseSlimeMap:
        AddDiffuseSlimeMapStage(renderGraph, ctx);
        break;
    case SlimeMoldPassStage::CopyDiffuse:
        AddCopyDiffuseSlimeMapStage(renderGraph, ctx);
        break;
    case SlimeMoldPassStage::Gradient:
        AddGradientStage(renderGraph, ctx);
        break;
    default:
        break;
    }
}

void SlimeMoldPass::AddUpdateSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo traitsBinding =
        m_UpdateSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_traits");
    static ShaderDescriptors::BindingInfo slimeBinding =
        m_UpdateSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_slime");
    static ShaderDescriptors::BindingInfo mapBinding =
        m_UpdateSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_slime_map");

    m_UpdateSlimeMapPass = &renderGraph.AddRenderPass<UpdateSlimeMapPassData>({"Slime.Update"},
        [&](Graph& graph, UpdateSlimeMapPassData& passData)
        {
            passData.Traits = graph.AddExternal("Slime.Update.Traits", ctx.GetTraitsBuffer());
            passData.Traits = graph.Read(passData.Traits, Compute | Storage);

            passData.Slime = graph.AddExternal("Slime.Update.Slime", ctx.GetSlimeBuffer());
            passData.Slime = graph.Read(passData.Slime, Compute | Storage);
            passData.Slime = graph.Write(passData.Slime, Compute | Storage);

            passData.SlimeMap = graph.AddExternal("Slime.Update.SlimeMap", ctx.GetSlimeMap());
            passData.SlimeMap = graph.Write(passData.SlimeMap, Compute | Storage);

            passData.PipelineData = &m_UpdateSlimeMapPipelineData;
            passData.PushConstants = &m_PushConstants;
            passData.SlimeMoldContext = &ctx;

            graph.GetBlackboard().Update(passData);
        },
        [=](UpdateSlimeMapPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Update slime");
            const Buffer& traitsBuffer = resources.GetBuffer(passData.Traits);
            const Buffer& slimeBuffer = resources.GetBuffer(passData.Slime);
            const Texture& slimeMap = resources.GetTexture(passData.SlimeMap);

            auto& pushConstant = *passData.PushConstants;
            auto& moldCtx = *passData.SlimeMoldContext;
            pushConstant.Width = (f32)moldCtx.GetBounds().x;
            pushConstant.Height = (f32)moldCtx.GetBounds().y;
            pushConstant.SlimeCount = (u32)moldCtx.GetSlime().size();
            // todo: fix me
            pushConstant.Dt = 1.0f / 60.0f;
            pushConstant.Time = (f32)frameContext.FrameNumberTick;
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(traitsBinding, traitsBuffer.BindingInfo());            
            resourceDescriptors.UpdateBinding(slimeBinding, slimeBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding(mapBinding, slimeMap.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            u32 slimeCount = (u32)moldCtx.GetSlime().size();
            u32 slimeCountDimension = (u32)std::sqrt((f32)slimeCount);
                        
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {slimeCountDimension + 1, slimeCountDimension, 1}, {16, 16, 1});
        });
}

void SlimeMoldPass::AddDiffuseSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo mapBinding =
        m_DiffuseSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_slime_map");
    static ShaderDescriptors::BindingInfo diffuseBinding =
        m_DiffuseSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_diffuse_map");

    m_DiffuseSlimeMapPass = &renderGraph.AddRenderPass<DiffuseSlimeMapPassData>({"Slime.Diffuse"},
        [&](Graph& graph, DiffuseSlimeMapPassData& passData)
        {
            passData.SlimeMap = graph.GetBlackboard().Get<UpdateSlimeMapPassData>().SlimeMap;
            passData.SlimeMap = graph.Read(passData.SlimeMap, Compute | Storage);

            passData.DiffuseMap = graph.CreateResource("Slime.Diffuse.DiffuseMap", GraphTextureDescription{
                .Width = ctx.GetBounds().x,
                .Height = ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.DiffuseMap = graph.Write(passData.DiffuseMap, Compute | Storage);

            passData.PipelineData = &m_DiffuseSlimeMapPipelineData;
            passData.PushConstants = &m_PushConstants;
            passData.SlimeMoldContext = &ctx;

            graph.GetBlackboard().Update(passData);
        },
        [=](DiffuseSlimeMapPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Diffuse slime");
            const Texture& slimeMap = resources.GetTexture(passData.SlimeMap);
            const Texture& diffuseMap = resources.GetTexture(passData.DiffuseMap);

            auto& pushConstant = *passData.PushConstants;
            auto& moldCtx = *passData.SlimeMoldContext;
            ImGui::Begin("slime diffuse");
            ImGui::DragFloat("diffuse rate", &pushConstant.DiffuseRate, 1.0f, 0.0f, 1000.0f);            
            ImGui::DragFloat("decay rate", &pushConstant.DecayRate, 1e-4f, 0.0f, 5.0f);
                ImGui::Begin("traits");
                auto& trait = moldCtx.GetTraits()[0];
                f32 sensorAngleDegrees = glm::degrees(trait.SensorAngle);
                ImGui::ColorEdit3("color", (f32*)&trait.Color);
                ImGui::DragFloat("movement speed", &trait.MovementSpeed, 10.0f, 100.0f, 10000.0f);
                ImGui::DragFloat("turning speed", &trait.TurningSpeed, 1.0f, 0.0f, 100.0f);
                ImGui::DragFloat("sensors angle", &sensorAngleDegrees, 1.0f, 0.0f, 180.0f);
                ImGui::DragFloat("sensors offset", &trait.SensorOffset, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("contagion chance", &trait.ContagionThreshold, 1e-4f, 0.0f, 1.0f);
                ImGui::DragInt("contagion steps", (i32*)&trait.ContagionSteps, 1, 1, 25);
                trait.SensorAngle = glm::radians(sensorAngleDegrees);
                if (ImGui::Button("Randomize"))
                    moldCtx.GetTraits()[0] = SlimeMoldContext::RandomTrait();
                ImGui::End();
            ImGui::End();
            moldCtx.UpdateTraits(*frameContext.ResourceUploader);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(mapBinding, slimeMap.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));       
            resourceDescriptors.UpdateBinding(diffuseBinding, diffuseMap.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1}, {16, 16, 1});
        });
}

void SlimeMoldPass::AddCopyDiffuseSlimeMapStage(RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    auto& diffuseOutput = renderGraph.GetBlackboard().Get<DiffuseSlimeMapPassData>();
    m_CopyDiffuseToMapPass->AddToGraph(renderGraph, diffuseOutput.DiffuseMap, diffuseOutput.SlimeMap,
        glm::vec3{}, glm::vec3{1.0f});
}

void SlimeMoldPass::AddGradientStage(RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo diffuseBinding =
        m_DiffuseSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_diffuse_map");
    static ShaderDescriptors::BindingInfo gradientBinding =
        m_DiffuseSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_gradient_map");
    static ShaderDescriptors::BindingInfo gradientColorsBinding =
        m_DiffuseSlimeMapPipelineData.ResourceDescriptors.GetBindingInfo("u_gradient_colors");

    m_GradientSlimeMapPass = &renderGraph.AddRenderPass<GradientPassData>({"Slime.Gradient"},
        [&](Graph& graph, GradientPassData& passData)
        {
            passData.DiffuseMap = graph.GetBlackboard().Get<DiffuseSlimeMapPassData>().DiffuseMap;
            passData.DiffuseMap = graph.Read(passData.DiffuseMap, Compute | Storage);

            passData.GradientMap = graph.CreateResource("Slime.Gradient.GradientMap", GraphTextureDescription{
                .Width = ctx.GetBounds().x,
                .Height = ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.GradientMap = graph.Write(passData.GradientMap, Compute | Storage);

            passData.Gradient = graph.CreateResource("Slime.Gradient.Colors", GraphBufferDescription{
                .SizeBytes = sizeof(GradientUBO)});
            passData.Gradient = graph.Read(passData.Gradient, Compute | Uniform | Upload);

            passData.PipelineData = &m_GradientSlimeMapPipelineData;
            passData.PushConstants = &m_PushConstants;
            passData.GradientData = &m_Gradient;
            passData.SlimeMoldContext = &ctx;

            graph.GetBlackboard().Update(passData);
        },
        [=](GradientPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Gradient slime");
            const Texture& diffuseMap = resources.GetTexture(passData.DiffuseMap);
            const Texture& gradientMap = resources.GetTexture(passData.GradientMap);

            auto& colors = *passData.GradientData;
            ImGui::Begin("slime gradient");
            ImGui::ColorEdit4("A", (f32*)&colors.A);
            ImGui::ColorEdit4("B", (f32*)&colors.B);
            ImGui::ColorEdit4("C", (f32*)&colors.C);
            ImGui::ColorEdit4("D", (f32*)&colors.D);
            if (ImGui::Button("Randomize"))
            {
                colors.A = Random::Float4();
                colors.B = Random::Float4();
                colors.C = Random::Float4();
                colors.D = Random::Float4();
            }
            ImGui::End();
            const Buffer& gradient = resources.GetBuffer(passData.Gradient, (void*)&colors, sizeof(GradientUBO), 0,
                *frameContext.ResourceUploader);
            frameContext.ResourceUploader->SubmitUpload(frameContext.Cmd);
            frameContext.ResourceUploader->StartRecording();
            
            auto& pushConstant = *passData.PushConstants;
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(diffuseBinding, diffuseMap.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(gradientBinding, gradientMap.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));
            resourceDescriptors.UpdateBinding(gradientColorsBinding, gradient.BindingInfo());

            auto& moldCtx = *passData.SlimeMoldContext;
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1}, {16, 16, 1});
        });
}
