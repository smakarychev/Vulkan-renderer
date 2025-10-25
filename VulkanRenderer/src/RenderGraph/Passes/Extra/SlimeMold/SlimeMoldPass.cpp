#include "rendererpch.h"

#include "SlimeMoldPass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "ResourceUploader.h"
#include "Math/Random.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SlimeBindGroup.generated.h"
#include "RenderGraph/Passes/Utility/CopyTexturePass.h"
#include "Rendering/Shader/ShaderCache.h"

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

    ctx.m_TraitsBuffer = Device::CreateBuffer({
        .SizeBytes = (u32)ctx.m_Traits.size() * sizeof(Traits),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage,
        .InitialData = ctx.m_Traits});

    ctx.m_SlimeBuffer = Device::CreateBuffer({
        .SizeBytes = (u32)ctx.m_Slime.size() * sizeof(Slime),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage,
        .InitialData = ctx.m_Slime});

    ctx.m_SlimeMap = Device::CreateImage({
            .Description = ImageDescription{
                .Width = bounds.x,
                .Height =  bounds.y,
                .Format = Format::RGBA16_FLOAT,
                .Usage = ImageUsage::Storage | ImageUsage::Destination}});

    resourceUploader.UpdateBuffer(ctx.m_SlimeBuffer, ctx.m_Slime);

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

namespace
{
struct PushConstants
{
    f32 Width;
    f32 Height;
    u32 SlimeCount;
    f32 Dt;
    f32 Time;
    f32 DiffuseRate;
    f32 DecayRate;

    static PushConstants FromContext(SlimeMoldContext& ctx, u64 frameTick)
    {
        PushConstants pushConstants = {};
        pushConstants.Width = (f32)ctx.GetBounds().x;
        pushConstants.Height = (f32)ctx.GetBounds().y;
        pushConstants.SlimeCount = (u32)ctx.GetSlime().size();
        pushConstants.Dt = 1.0f / 60.0f;
        pushConstants.Time = (f32)frameTick;
        pushConstants.DecayRate = ctx.GetDecayRate();
        pushConstants.DiffuseRate = ctx.GetDiffuseRate();

        return pushConstants;
    }
};
struct GradientUBO
{
    glm::vec4 A{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 B{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 C{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 D{0.0f, 0.1f, 0.2f, 1.0f};
};
struct UpdateSlimeMapPassData
{
    RG::Resource Traits{};
    RG::Resource Slime{};
    RG::Resource SlimeMap{};
    SlimeMoldContext* SlimeMoldContext{nullptr};
};
struct DiffuseSlimeMapPassData
{
    RG::Resource SlimeMap{};
    RG::Resource DiffuseMap{};

    SlimeMoldContext* SlimeMoldContext{nullptr};
};
struct GradientPassData
{
    RG::Resource DiffuseMap{};
    RG::Resource GradientMap{};
    RG::Resource Gradient{};

    SlimeMoldContext* SlimeMoldContext{ nullptr };
};
    
UpdateSlimeMapPassData& addUpdateSlimeMapStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<UpdateSlimeMapPassData>(name.Concatenate(".Update"),
        [&](Graph& graph, UpdateSlimeMapPassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Update.Setup")
            
            graph.SetShader("slime"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"SLIME_MAP_STAGE"_hsv, true}});
            
            passData.Traits = graph.Import("Update.Traits"_hsv, ctx.GetTraitsBuffer());
            passData.Traits = graph.ReadBuffer(passData.Traits, Compute | Storage);

            passData.Slime = graph.Import("Update.Slime"_hsv, ctx.GetSlimeBuffer());
            passData.Slime = graph.ReadWriteBuffer(passData.Slime, Compute | Storage);

            passData.SlimeMap = graph.Import("Update.SlimeMap"_hsv, ctx.GetSlimeMap());
            passData.SlimeMap = graph.WriteImage(passData.SlimeMap, Compute | Storage);

            passData.SlimeMoldContext = &ctx;

            ImGui::Begin("slime diffuse");
            ImGui::DragFloat("diffuse rate", &ctx.GetDiffuseRate(), 1.0f, 0.0f, 1000.0f);            
            ImGui::DragFloat("decay rate", &ctx.GetDecayRate(), 1e-4f, 0.0f, 5.0f);
                ImGui::Begin("traits");
                auto& trait = ctx.GetTraits()[0];
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
                    ctx.GetTraits()[0] = SlimeMoldContext::RandomTrait();
                ImGui::End();
            ImGui::End();
            passData.Traits = graph.Upload(passData.Traits, ctx.GetTraits());
        },
        [=](const UpdateSlimeMapPassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Slime.Update")
            GPU_PROFILE_FRAME("Slime.Update")

            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);

            const Shader& shader = graph.GetShader();
            SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetTraits(graph.GetBufferBinding(passData.Traits));
            bindGroup.SetSlime(graph.GetBufferBinding(passData.Slime));
            bindGroup.SetSlimeMap(graph.GetImageBinding(passData.SlimeMap));

            u32 slimeCount = (u32)moldCtx.GetSlime().size();
            u32 slimeCountDimension = (u32)std::sqrt((f32)slimeCount);
                        
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = {slimeCountDimension + 1, slimeCountDimension, 1},
	            .GroupSize = {16, 16, 1}});
        });
}

DiffuseSlimeMapPassData& addDiffuseSlimeMapStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx,
    const UpdateSlimeMapPassData& updateOutput)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<DiffuseSlimeMapPassData>(name.Concatenate(".Diffuse"),
        [&](Graph& graph, DiffuseSlimeMapPassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Diffuse.Setup")
            
            graph.SetShader("slime"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"SLIME_DIFFUSE_STAGE"_hsv, true}});

            passData.SlimeMap = updateOutput.SlimeMap;
            passData.SlimeMap = graph.ReadImage(passData.SlimeMap, Compute | Storage);

            passData.DiffuseMap = graph.Create("Diffuse.DiffuseMap"_hsv, RGImageDescription{
                .Width = (f32)ctx.GetBounds().x,
                .Height = (f32)ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.DiffuseMap = graph.WriteImage(passData.DiffuseMap, Compute | Storage);

            passData.SlimeMoldContext = &ctx;
        },
        [=](const DiffuseSlimeMapPassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Slime.Diffuse")
            GPU_PROFILE_FRAME("Slime.Diffuse")
            
            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);
            
            const Shader& shader = graph.GetShader();
            SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetSlimeMap(graph.GetImageBinding(passData.SlimeMap));
            bindGroup.SetDiffuseMap(graph.GetImageBinding(passData.DiffuseMap));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1},
	            .GroupSize = {16, 16, 1}});
        });
}

Passes::CopyTexture::PassData& addCopyDiffuseSlimeMapStage(StringId name, RG::Graph& renderGraph,
    const DiffuseSlimeMapPassData& diffuseOutput)
{
    return Passes::CopyTexture::addToGraph(name.Concatenate(".Copy"), renderGraph, {
        .TextureIn = diffuseOutput.DiffuseMap,
        .TextureOut = diffuseOutput.SlimeMap});
}

GradientPassData& addGradientStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx,
    const DiffuseSlimeMapPassData& diffuseOutput)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<GradientPassData>(name.Concatenate(".Gradient"),
        [&](Graph& graph, GradientPassData& passData)
        {
            CPU_PROFILE_FRAME("Gradient.Slime.Setup")

            graph.SetShader("slime"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"SLIME_GRADIENT_STAGE"_hsv, true}});

            passData.DiffuseMap = diffuseOutput.DiffuseMap;
            passData.DiffuseMap = graph.ReadImage(passData.DiffuseMap, Compute | Storage);

            passData.GradientMap = graph.Create("Gradient.GradientMap"_hsv, RGImageDescription{
                .Width = (f32)ctx.GetBounds().x,
                .Height = (f32)ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.GradientMap = graph.WriteImage(passData.GradientMap, Compute | Storage);

            passData.Gradient = graph.Create("Gradient.Colors"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(GradientUBO)});
            passData.Gradient = graph.ReadImage(passData.Gradient, Compute | Uniform);

            auto& gradient = graph.GetOrCreateBlackboardValue<GradientUBO>();
            ImGui::Begin("slime gradient");
            ImGui::ColorEdit4("A", (f32*)&gradient.A);
            ImGui::ColorEdit4("B", (f32*)&gradient.B);
            ImGui::ColorEdit4("C", (f32*)&gradient.C);
            ImGui::ColorEdit4("D", (f32*)&gradient.D);
            if (ImGui::Button("Randomize"))
            {
                gradient.A = Random::Float4();
                gradient.B = Random::Float4();
                gradient.C = Random::Float4();
                gradient.D = Random::Float4();
            }
            ImGui::End();
            passData.Gradient = graph.Upload(passData.Gradient, gradient);

            passData.SlimeMoldContext = &ctx;
        },
        [=](const GradientPassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Gradient.Slime")
            GPU_PROFILE_FRAME("Gradient.Slime")
            
            const Shader& shader = graph.GetShader();
            SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetDiffuseMap(graph.GetImageBinding(passData.DiffuseMap));
            bindGroup.SetGradientMap(graph.GetImageBinding(passData.GradientMap));
            bindGroup.SetGradientColors(graph.GetBufferBinding(passData.Gradient));

            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1},
	            .GroupSize = {16, 16, 1}});
        });
}
}

Passes::SlimeMold::PassData& Passes::SlimeMold::addToGraph(StringId name, RG::Graph& renderGraph,
    SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Setup")

            auto& update = addUpdateSlimeMapStage(name, graph, ctx);
            auto& diffuse = addDiffuseSlimeMapStage(name, graph, ctx, update);
            auto& gradient = addGradientStage(name, graph, ctx, diffuse);

            passData.ColorOut = gradient.GradientMap;

            addCopyDiffuseSlimeMapStage(name, graph, diffuse);
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}