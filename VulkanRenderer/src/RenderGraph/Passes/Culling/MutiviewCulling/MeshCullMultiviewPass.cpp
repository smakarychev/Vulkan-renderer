#include "MeshCullMultiviewPass.h"

#include "FrameContext.h"
#include "RenderGraph/Passes/Generated/MeshCullMultiviewBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"

RG::Pass& Passes::Multiview::MeshCull::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const MeshCullMultiviewPassExecutionInfo& info, CullStage stage)
{
    using namespace RG;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Mesh.Cull.Multiview.Setup")

            graph.SetShader("mesh-cull-multiview.shader",
                ShaderOverrides{
                    ShaderOverride{{"REOCCLUSION"}, stage == CullStage::Reocclusion},
                    ShaderOverride{{"SINGLE_PASS"}, stage == CullStage::Single}});
            
            if (stage != CullStage::Cull)
                for (u32 i = 0; i < info.MultiviewResource->ViewCount; i++)
                    info.MultiviewResource->HiZs[i] =
                        info.MultiviewResource->Multiview->View(i).Static.HiZContext->GetHiZResource(
                            HiZReductionMode::Min);

            RgUtils::readWriteCullMeshMultiview(*info.MultiviewResource, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Mesh.Cull.Multiview")
            GPU_PROFILE_FRAME("Mesh.Cull.Multiview")

            auto* multiview = passData.MultiviewResource;
            Sampler hizSampler = multiview->HiZSampler;
            
            const Shader& shader = resources.GetGraph()->GetShader();
            MeshCullMultiviewShaderBindGroup bindGroup(shader);
            bindGroup.SetSampler(hizSampler);

            RgUtils::updateMeshCullMultiviewBindings(bindGroup, resources, *multiview);

            struct PushConstant
            {
                u32 ObjectCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());

            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshCount = multiview->Multiview->View(i).Static.Geometry->GetRenderObjectCount();
                
                PushConstant pushConstant = {
                    .ObjectCount = meshCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                frameContext.CommandList.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});

                frameContext.CommandList.Dispatch({
				    .Invocations = {meshCount, 1, 1},
				    .GroupSize = {64, 1, 1}});
            }
        });

    return pass;
}
