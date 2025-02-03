#include "MeshletCullTranslucentMultiviewPass.h"

#include "CullMultiviewResources.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/MeshletCullTranslucentMultiviewBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"

RG::Pass& Passes::Multiview::MeshletCullTranslucent::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const MeshletCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Meshlet.Cull.Multiview.Translucent.Setup")

            graph.SetShader("meshlet-cull-translucent-multiview.shader", {});
            
            RgUtils::readWriteCullMeshletMultiview(*info.MultiviewResource, CullStage::Single, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Meshlet.Cull.Multiview.Translucent")
            GPU_PROFILE_FRAME("Meshlet.Cull.Multiview.Translucent")

            auto* multiview = passData.MultiviewResource;

            Sampler hizSampler = multiview->HiZSampler;
            
            const Shader& shader = resources.GetGraph()->GetShader();
            MeshletCullTranslucentMultiviewShaderBindGroup bindGroup(shader);
            bindGroup.SetSampler(hizSampler);

            RgUtils::updateCullMeshletMultiviewBindings(bindGroup, resources, *multiview, CullStage::Single);

            struct PushConstant
            {
                u32 MeshletCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshletCount = multiview->Multiview->View(i).Static.Geometry->GetMeshletCount();
                
                PushConstant pushConstant = {
                    .MeshletCount = meshletCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                frameContext.CommandList.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});

                frameContext.CommandList.Dispatch({
				    .Invocations = {meshletCount, 1, 1},
				    .GroupSize = {64, 1, 1}});
            }
        });

    return pass;
}
