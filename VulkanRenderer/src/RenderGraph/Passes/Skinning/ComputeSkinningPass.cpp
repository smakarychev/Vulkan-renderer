#include "rendererpch.h"
#include "ComputeSkinningPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SkinningBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SkinningPrepareBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SkinningUpdateBoundsBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/Types/SkinDispatchInfoUniform.generated.h"
#include "Scene/SceneGeometry.h"

namespace 
{
struct PrepareSkinningPassData
{
    RG::Resource SkinDispatchCount{};
    RG::Resource SkinDispatchesInfo{};
    RG::Resource RenderObjects{};
    u32 DispatchCount{};
};
PrepareSkinningPassData& prepare(StringId name, RG::Graph& renderGraph,
    const Passes::ComputeSkinning::ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PrepareSkinningPassData, SkinningPrepareBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Skinning.Prepare.Setup")

            passData.BindGroup = SkinningPrepareBindGroupRG(graph);
            
            passData.DispatchCount = info.SkinnedMeshletCount;
           
            Resource skinDispatchCount = graph.Create("VertexInfoCount"_hsv, 
                RGBufferDescription{.SizeBytes = sizeof(u32)});
            skinDispatchCount = graph.Upload(skinDispatchCount, 0);
            Resource skinDispatchesInfo = graph.Create("DispatchesInfo"_hsv, RGBufferDescription{
                .SizeBytes = 
                    sizeof(::gen::SkinDispatchInfo) * passData.DispatchCount
                });
            
            passData.BindGroup.SetResourcesMeshletsUgb(info.Meshlets);
            passData.BindGroup.SetResourcesSkins(info.Skins);
            passData.BindGroup.SetResourcesSkinnedRenderObjectInfo(info.RenderObjectSkinnedInfos);
            passData.BindGroup.SetResourcesSkinnedRenderObjectInfoIndices(info.RenderObjectSkinnedInfoIndices);
            
            passData.RenderObjects = passData.BindGroup.SetResourcesObjects(info.RenderObjects);
            passData.SkinDispatchCount = 
                passData.BindGroup.SetResourcesSkinDispatchCount(skinDispatchCount);
            passData.SkinDispatchesInfo = passData.BindGroup.SetResourcesSkinDispatchesInfo(skinDispatchesInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Skinning.Prepare")
            GPU_PROFILE_FRAME("Skinning.Prepare")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {info.SkinnedRenderObjectCount}
            });
            cmd.Dispatch({
               .Invocations = {info.SkinnedRenderObjectCount, 1, 1},
               .GroupSize = passData.BindGroup.GetPrepareComputeSkinningGroupSize()
            });
        });
}

struct VertexSkinningPassData
{
    RG::Resource RenderObjects{};
    RG::Resource Ugb{};
    RG::Resource Meshlets{};
};

VertexSkinningPassData& skinVertices(StringId name, RG::Graph& renderGraph,
    const Passes::ComputeSkinning::ExecutionInfo& info, const PrepareSkinningPassData& prepareSkinning)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<VertexSkinningPassData, SkinningBindGroupRG>;
    
    const u32 dispatchCount = prepareSkinning.DispatchCount;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Skinning.Setup")

            passData.BindGroup = SkinningBindGroupRG(graph);
            
            passData.BindGroup.SetResourcesSkinDispatchesInfo(prepareSkinning.SkinDispatchesInfo);
            passData.BindGroup.SetResourcesSkinDispatchCount(prepareSkinning.SkinDispatchCount);
            passData.BindGroup.SetResourcesJointMatrices(info.JointMatrices);
            
            passData.RenderObjects = prepareSkinning.RenderObjects;
            passData.Meshlets = passData.BindGroup.SetResourcesMeshletsUgb(info.Meshlets);
            passData.Ugb = passData.BindGroup.SetResourcesUgb(info.Ugb);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Skinning")
            GPU_PROFILE_FRAME("Skinning")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
               .Invocations = {dispatchCount, 1, 1},
            });
        });
}
Passes::ComputeSkinning::PassData& updateBounds(StringId name, RG::Graph& renderGraph,
    const Passes::ComputeSkinning::ExecutionInfo& info,
    const VertexSkinningPassData& vertexSkinning)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<Passes::ComputeSkinning::PassData, SkinningUpdateBoundsBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Skinning.UpdateBounds.Setup")

            passData.BindGroup = SkinningUpdateBoundsBindGroupRG(graph);
            
            passData.RenderObjects = passData.BindGroup.SetResourcesObjects(vertexSkinning.RenderObjects);
            passData.Meshlets = passData.BindGroup.SetResourcesMeshletsUgb(vertexSkinning.Meshlets);
            passData.BindGroup.SetResourcesSkinnedRenderObjectInfo(info.RenderObjectSkinnedInfos);
            passData.BindGroup.SetResourcesSkinnedRenderObjectInfoIndices(info.RenderObjectSkinnedInfoIndices);
            passData.Ugb = vertexSkinning.Ugb;
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Skinning.UpdateBounds")
            GPU_PROFILE_FRAME("Skinning.UpdateBounds")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {info.SkinnedRenderObjectCount}
            });
            cmd.Dispatch({
               .Invocations = {info.SkinnedRenderObjectCount, 1, 1},
            });
        });
}
}

Passes::ComputeSkinning::PassData& Passes::ComputeSkinning::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    auto& prepareSkinning = prepare(name.Concatenate(".Prepare"), renderGraph, info);
    auto& vertexSkinning = skinVertices(name, renderGraph, info, prepareSkinning);
    
    return updateBounds(name.Concatenate(".UpdateBounds"), renderGraph, info, vertexSkinning);
}
