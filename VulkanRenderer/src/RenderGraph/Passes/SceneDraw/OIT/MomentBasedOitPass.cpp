#include "rendererpch.h"
#include "MomentBasedOitPass.h"

#include "RenderGraph/Passes/Generated/SceneDrawMomentBasedOitCompositeBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneDrawMomentBasedOitDepthBoundsBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneDrawMomentBasedOitDepthBoundsDispatchBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneDrawMomentBasedOitForwardPbrBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneDrawMomentBasedOitTransmittanceBindGroupRG.generated.h"
#include "RenderGraph/Passes/Scene/SceneGeometryRGResources.h"
#include "RenderGraph/Passes/Utility/ClearTexturePass.h"


namespace Passes::MomentBasedOit
{
namespace
{
RG::BufferResource prepareBoundsDispatch(StringId name, RG::Graph& renderGraph, RG::BufferResource drawInfo)
{
    using namespace RG;
    struct PassDataBind
    {
        SceneDrawMomentBasedOitDepthBoundsDispatchBindGroupRG BindGroup;
        BufferResource Dispatch{};
    };
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.BoundsDispatch.Setup")

            passData.BindGroup = SceneDrawMomentBasedOitDepthBoundsDispatchBindGroupRG(graph);

            passData.Dispatch = graph.Create("Dispatch"_hsv,
                RGBufferDescription{.SizeBytes = sizeof(DispatchIndirectCommand)});
            passData.Dispatch = passData.BindGroup.SetResourcesCommand(
                passData.Dispatch, ResourceAccessFlags::Indirect);
            passData.BindGroup.SetResourcesDrawInfo(drawInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.BoundsDispatch")
            GPU_PROFILE_FRAME("Scene.MomentBasedOit.BoundsDispatch")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({.Invocations = {1, 1, 1}});
        }).Dispatch;
}
RG::BufferResource calculateBounds(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    struct PassDataBind
    {
        SceneDrawMomentBasedOitDepthBoundsBindGroupRG BindGroup;
        BufferResource DepthRange{};
    };

    const BufferResource dispatch = prepareBoundsDispatch(name.Concatenate(".Dispatch"), renderGraph,
        info.DrawInfo.DrawInfo);
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Bounds.Setup")

            passData.BindGroup = SceneDrawMomentBasedOitDepthBoundsBindGroupRG(graph);
            
            passData.DepthRange = graph.Create("DepthRange"_hsv, RGBufferDescription{.SizeBytes = sizeof(glm::uvec2)});
            passData.DepthRange = graph.Upload(passData.DepthRange, glm::uvec2(~0u, 0u));
            
            passData.DepthRange = passData.BindGroup.SetResourcesDepthRange(passData.DepthRange);
            passData.BindGroup.SetResourcesDrawInfos(info.DrawInfo.DrawInfo);
            passData.BindGroup.SetResourcesUgb(info.Geometry->Attributes);
            passData.BindGroup.SetResourcesRenderObjects(info.Geometry->RenderObjects);
            passData.BindGroup.SetResourcesMeshletsUgb(info.Geometry->Meshlets);
            passData.BindGroup.SetResourcesVisibleMeshlets(info.DrawInfo.VisibleMeshlets);
            passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Bounds")
            GPU_PROFILE_FRAME("Scene.MomentBasedOit.Bounds")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.DispatchIndirect({.Buffer = graph.GetBuffer(dispatch)});
        }).DepthRange;
}
struct TransmittancePassData
{
    SceneDrawPassResources Resources{};
    RG::BufferResource DepthRange{};
    RG::ImageResource AbsorbanceTotal{};
    RG::ImageResource AbsorbanceMoments{};
};
TransmittancePassData& transmittancePass(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<TransmittancePassData, SceneDrawMomentBasedOitTransmittanceBindGroupRG>;
    
    const BufferResource depthBounds = calculateBounds(name.Concatenate("DepthBounds"), renderGraph, info);

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Transmittance.Setup")

            passData.BindGroup = SceneDrawMomentBasedOitTransmittanceBindGroupRG(graph,
                info.CommonOverrides.has_value() ?
                info.CommonOverrides->OverrideBy(*info.DrawInfo.BucketOverrides) :
                *info.DrawInfo.BucketOverrides);

            ImageResource absorbanceTotal = graph.Create("AbsorbanceTotal"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = info.ColorIn,
                .Format = Format::R16_FLOAT
            });
            ImageResource absorbanceMoments = graph.Create("AbsorbanceMoments"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = info.ColorIn,
                .Format = Format::RGBA16_FLOAT
            });
            absorbanceTotal = ClearColorTexture::addToGraph(name.Concatenate(".ClearAbsorbance"), graph, {
                .Image = absorbanceTotal,
                .ClearValue = {.F = glm::vec4(0.0f)}
            }).Image;
            absorbanceMoments = ClearColorTexture::addToGraph(name.Concatenate(".ClearAbsorbanceMoments"), graph, {
                .Image = absorbanceMoments,
                .ClearValue = {.F = glm::vec4(0.0f)}
            }).Image;
            
            auto drawInfo = info.DrawInfo;
            drawInfo.Attachments.Colors.clear();
            passData.Resources.InitFrom(drawInfo, graph);
            passData.BindGroup.SetResourcesUgb(info.Geometry->Attributes);
            passData.BindGroup.SetResourcesRenderObjects(info.Geometry->RenderObjects);
            passData.BindGroup.SetResourcesMeshletsUgb(info.Geometry->Meshlets);
            passData.Resources.VisibleMeshlets =
                passData.BindGroup.SetResourcesVisibleMeshlets(passData.Resources.VisibleMeshlets);
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
            passData.BindGroup.SetResourcesMaterials(info.Geometry->Materials);
            
            passData.DepthRange = passData.BindGroup.SetResourcesDepthRange(depthBounds);
            passData.AbsorbanceTotal = passData.BindGroup.SetResourcesAbsorbanceTotal(absorbanceTotal);
            passData.AbsorbanceMoments = passData.BindGroup.SetResourcesAbsorbanceMoments(absorbanceMoments);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Transmittance")
            GPU_PROFILE_FRAME("Scene.MomentBasedOit.Transmittance")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.BindIndexU8Buffer({
                .Buffer = graph.GetBuffer(info.Geometry->Indices)
            });
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount
            });
        });
}
struct DrawPassData
{
    SceneDrawPassResources Resources{};
    RG::ImageResource AbsorbanceTotal{};
    RG::ImageResource TranslucentColor{};
};
DrawPassData& drawPass(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<DrawPassData, SceneDrawMomentBasedOitForwardPbrBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Forward.Setup")
            
            const bool useHybrid = info.Tiles.IsValid() && info.Clusters.IsValid();
            const bool useTiled = !useHybrid && info.Tiles.IsValid();
            const bool useClustered = !useHybrid && info.Clusters.IsValid();

            auto variant = SceneDrawMomentBasedOitForwardPbrBindGroupRG::Variants::Hybrid;
            if (useTiled)
                variant = SceneDrawMomentBasedOitForwardPbrBindGroupRG::Variants::Tiled;
            if (useClustered)
                variant = SceneDrawMomentBasedOitForwardPbrBindGroupRG::Variants::Clustered;

            passData.BindGroup = SceneDrawMomentBasedOitForwardPbrBindGroupRG(graph, variant,
                info.CommonOverrides.has_value() ?
                info.CommonOverrides->OverrideBy(*info.DrawInfo.BucketOverrides) :
                *info.DrawInfo.BucketOverrides);

            auto& transmittance = transmittancePass(name.Concatenate(".Transmittance"), graph, info);
            
            passData.Resources.InitFrom(info.DrawInfo, graph);
            passData.BindGroup.SetResourcesUgb(info.Geometry->Attributes);
            passData.BindGroup.SetResourcesRenderObjects(info.Geometry->RenderObjects);
            passData.BindGroup.SetResourcesMeshletsUgb(info.Geometry->Meshlets);
            passData.Resources.VisibleMeshlets =
                passData.BindGroup.SetResourcesVisibleMeshlets(passData.Resources.VisibleMeshlets);
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
            passData.BindGroup.SetResourcesMaterials(info.Geometry->Materials);
            passData.BindGroup.SetResourcesCsmData(info.CsmData.CsmInfo);
            passData.BindGroup.SetResourcesCsmTexture(info.CsmData.ShadowMap);
            passData.BindGroup.SetResourcesSsao(info.SSAO.SSAO);
            passData.BindGroup.SetResourcesDirectionalLights(info.DirectionalLights);
            passData.BindGroup.SetResourcesPointLights(info.PointLights);
            if (useTiled || useHybrid)
            {
                passData.BindGroup.SetResourcesLightZBins(info.ZBins);
                passData.BindGroup.SetResourcesLightTiles(info.Tiles);
            }
            if (useClustered || useHybrid)
            {
                passData.BindGroup.SetResourcesLightClusters(info.Clusters);
            }
            passData.BindGroup.SetResourcesPrefilteredEnvironment(info.IBL.PrefilterEnvironment);
            passData.BindGroup.SetResourcesBrdf(info.IBL.BRDF);
            passData.BindGroup.SetResourcesIrradianceSH(info.IBL.IrradianceSH);

            passData.BindGroup.SetResourcesDepthRange(transmittance.DepthRange);
            
            passData.BindGroup.SetResourcesAbsorbanceMoments(transmittance.AbsorbanceMoments);
            passData.AbsorbanceTotal = passData.BindGroup.SetResourcesAbsorbanceTotal(transmittance.AbsorbanceTotal);
            passData.TranslucentColor = passData.Resources.Attachments.Colors[0];
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Forward")
            GPU_PROFILE_FRAME("Scene.MomentBasedOit.Forward")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.BindIndexU8Buffer({
                .Buffer = graph.GetBuffer(info.Geometry->Indices)
            });
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount
            });
        });
}
}
}

Passes::MomentBasedOit::PassData& Passes::MomentBasedOit::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneDrawMomentBasedOitCompositeBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit.Setup")
            
            passData.BindGroup = SceneDrawMomentBasedOitCompositeBindGroupRG(graph);
            
            auto& draw = drawPass(name.Concatenate(".Draw"), graph, info);
            passData.BindGroup.SetResourcesAbsorbanceTotal(draw.AbsorbanceTotal);
            passData.BindGroup.SetResourcesTranslucent(draw.TranslucentColor);
            passData.Resources = draw.Resources;
            passData.Color = passData.BindGroup.SetResourcesSceneColor(info.ColorIn);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.MomentBasedOit")
            GPU_PROFILE_FRAME("Scene.MomentBasedOit")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.Color).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetComputeMainGroupSize()
            });
        });
}
