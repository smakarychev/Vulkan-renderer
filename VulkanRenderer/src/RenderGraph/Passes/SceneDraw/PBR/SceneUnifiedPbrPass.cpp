#include "SceneUnifiedPbrPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneUgbBindGroup.generated.h"
#include "Scene/Scene.h"

RG::Pass& Passes::SceneUnifiedPbr::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        SceneDrawPassResources Resources{};
        Resource UGB{};
        Resource Objects{};
        SceneLightResources Light{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.DrawUnifiedBasic.Setup")

            graph.SetShader("scene-ugb.shader", *info.DrawInfo.Overrides);

            passData.Resources.CreateFrom(info.DrawInfo, graph);

            passData.UGB = graph.AddExternal("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.Read(passData.Objects, Vertex | Pixel | Storage);

            passData.Light = RgUtils::readSceneLight(*info.Lights, graph, Pixel);
            
            PassData passDataPublic = {};
            passDataPublic.Attachments = passData.Resources.Attachments;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.DrawUnifiedBasic")
            GPU_PROFILE_FRAME("Scene.DrawUnifiedBasic")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Resources.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Resources.Draws)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});
            bindGroup.SetDirectionalLights({.Buffer = resources.GetBuffer(passData.Light.DirectionalLights)});
            bindGroup.SetPointLights({.Buffer = resources.GetBuffer(passData.Light.PointLights)});
            bindGroup.SetLightsInfo({.Buffer = resources.GetBuffer(passData.Light.LightsInfo)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = resources.GetBuffer(passData.Resources.Draws),
                .CountBuffer = resources.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        });
}
