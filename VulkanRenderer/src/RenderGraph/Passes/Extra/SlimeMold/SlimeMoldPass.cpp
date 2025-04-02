#include "SlimeMoldPass.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "Core/Random.h"
#include "imgui/imgui.h"
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
    
RG::Pass& addUpdateSlimeMapStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<UpdateSlimeMapPassData>(name.Concatenate(".Update"),
        [&](Graph& graph, UpdateSlimeMapPassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Update.Setup")
            
            graph.SetShader("slime.shader",
                ShaderOverrides{
                    ShaderOverride{"SLIME_MAP_STAGE"_hsv, true}});
            
            passData.Traits = graph.AddExternal("Update.Traits"_hsv, ctx.GetTraitsBuffer());
            passData.Traits = graph.Read(passData.Traits, Compute | Storage);

            passData.Slime = graph.AddExternal("Update.Slime"_hsv, ctx.GetSlimeBuffer());
            passData.Slime = graph.Read(passData.Slime, Compute | Storage);
            passData.Slime = graph.Write(passData.Slime, Compute | Storage);

            passData.SlimeMap = graph.AddExternal("Update.SlimeMap"_hsv, ctx.GetSlimeMap());
            passData.SlimeMap = graph.Write(passData.SlimeMap, Compute | Storage);

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
            graph.Upload(passData.Traits, ctx.GetTraits());

            graph.UpdateBlackboard(passData);
        },
        [=](UpdateSlimeMapPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Slime.Update")
            GPU_PROFILE_FRAME("Slime.Update")
            Buffer traitsBuffer = resources.GetBuffer(passData.Traits);
            Buffer slimeBuffer = resources.GetBuffer(passData.Slime);
            Texture slimeMap = resources.GetTexture(passData.SlimeMap);

            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);

            const Shader& shader = resources.GetGraph()->GetShader();SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetTraits({.Buffer = traitsBuffer});
            bindGroup.SetSlime({.Buffer = slimeBuffer});
            bindGroup.SetSlimeMap({.Image = slimeMap}, ImageLayout::General);

            u32 slimeCount = (u32)moldCtx.GetSlime().size();
            u32 slimeCountDimension = (u32)std::sqrt((f32)slimeCount);
                        
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = {slimeCountDimension + 1, slimeCountDimension, 1},
	            .GroupSize = {16, 16, 1}});
        });
}

RG::Pass& addDiffuseSlimeMapStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx,
    const UpdateSlimeMapPassData& updateOutput)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<DiffuseSlimeMapPassData>(name.Concatenate(".Diffuse"),
        [&](Graph& graph, DiffuseSlimeMapPassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Diffuse.Setup")
            
            graph.SetShader("slime.shader",
                ShaderOverrides{
                    ShaderOverride{"SLIME_DIFFUSE_STAGE"_hsv, true}});

            passData.SlimeMap = updateOutput.SlimeMap;
            passData.SlimeMap = graph.Read(passData.SlimeMap, Compute | Storage);

            passData.DiffuseMap = graph.CreateResource("Diffuse.DiffuseMap"_hsv, GraphTextureDescription{
                .Width = ctx.GetBounds().x,
                .Height = ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.DiffuseMap = graph.Write(passData.DiffuseMap, Compute | Storage);

            passData.SlimeMoldContext = &ctx;

            graph.UpdateBlackboard(passData);
        },
        [=](DiffuseSlimeMapPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Slime.Diffuse")
            GPU_PROFILE_FRAME("Slime.Diffuse")
            
            Texture slimeMap = resources.GetTexture(passData.SlimeMap);
            Texture diffuseMap = resources.GetTexture(passData.DiffuseMap);

            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetSlimeMap({.Image = slimeMap}, ImageLayout::Readonly);
            bindGroup.SetDiffuseMap({.Image = diffuseMap}, ImageLayout::General);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1},
	            .GroupSize = {16, 16, 1}});
        });
}

RG::Pass& addCopyDiffuseSlimeMapStage(StringId name, RG::Graph& renderGraph,
    const DiffuseSlimeMapPassData& diffuseOutput)
{
    return Passes::CopyTexture::addToGraph(name.Concatenate(".Copy"), renderGraph,
        diffuseOutput.DiffuseMap, diffuseOutput.SlimeMap, glm::vec3{}, glm::vec3{1.0f});
}

RG::Pass& addGradientStage(StringId name, RG::Graph& renderGraph, SlimeMoldContext& ctx,
    const DiffuseSlimeMapPassData& diffuseOutput)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<GradientPassData>(name.Concatenate(".Gradient"),
        [&](Graph& graph, GradientPassData& passData)
        {
            CPU_PROFILE_FRAME("Gradient.Slime.Setup")

            graph.SetShader("slime.shader",
                ShaderOverrides{
                    ShaderOverride{"SLIME_GRADIENT_STAGE"_hsv, true}});

            passData.DiffuseMap = diffuseOutput.DiffuseMap;
            passData.DiffuseMap = graph.Read(passData.DiffuseMap, Compute | Storage);

            passData.GradientMap = graph.CreateResource("Gradient.GradientMap"_hsv, GraphTextureDescription{
                .Width = ctx.GetBounds().x,
                .Height = ctx.GetBounds().y,
                .Format = Format::RGBA16_FLOAT});
            passData.GradientMap = graph.Write(passData.GradientMap, Compute | Storage);

            passData.Gradient = graph.CreateResource("Gradient.Colors"_hsv, GraphBufferDescription{
                .SizeBytes = sizeof(GradientUBO)});
            passData.Gradient = graph.Read(passData.Gradient, Compute | Uniform);

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
            graph.Upload(passData.Gradient, gradient);

            passData.SlimeMoldContext = &ctx;

            graph.UpdateBlackboard(passData);
        },
        [=](GradientPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Gradient.Slime")
            GPU_PROFILE_FRAME("Gradient.Slime")
            Texture diffuseMap = resources.GetTexture(passData.DiffuseMap);
            Texture gradientMap = resources.GetTexture(passData.GradientMap);
            
            Buffer gradient = resources.GetBuffer(passData.Gradient);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            SlimeShaderBindGroup bindGroup(shader);
            bindGroup.SetDiffuseMap({.Image = diffuseMap}, ImageLayout::Readonly);
            bindGroup.SetGradientMap({.Image = gradientMap}, ImageLayout::General);
            bindGroup.SetGradientColors({.Buffer = gradient});

            auto& moldCtx = *passData.SlimeMoldContext;
            PushConstants pushConstants = PushConstants::FromContext(moldCtx, frameContext.FrameNumberTick);
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Dispatch({
	            .Invocations = { moldCtx.GetBounds().x, moldCtx.GetBounds().y, 1},
	            .GroupSize = {16, 16, 1}});
        });
}
}

RG::Pass& Passes::SlimeMold::addToGraph(StringId name, RG::Graph& renderGraph,
    SlimeMoldContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Slime.Setup")

            auto& update = addUpdateSlimeMapStage(name, graph, ctx);
            auto& diffuse = addDiffuseSlimeMapStage(name, graph, ctx,
                graph.GetBlackboard().Get<UpdateSlimeMapPassData>(update));
            auto& gradient = addGradientStage(name, graph, ctx,
                graph.GetBlackboard().Get<DiffuseSlimeMapPassData>(diffuse));

            passData.ColorOut = graph.GetBlackboard().Get<GradientPassData>(gradient).GradientMap;

            addCopyDiffuseSlimeMapStage(name, graph,
                graph.GetBlackboard().Get<DiffuseSlimeMapPassData>(diffuse));

            graph.UpdateBlackboard(passData);
        },
        [=](PassData&, FrameContext&, const Resources&){});
}