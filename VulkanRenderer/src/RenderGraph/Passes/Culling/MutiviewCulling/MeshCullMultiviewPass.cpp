#include "MeshCullMultiviewPass.h"

#include "FrameContext.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

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

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            Sampler hizSampler = multiview->HiZSampler;
            samplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                multiview->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

            RgUtils::updateMeshCullMultiviewBindings(resourceDescriptors, resources, *multiview);

            struct PushConstant
            {
                u32 ObjectCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.Cmd;
            RenderCommand::BindCompute(cmd, pipeline);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshCount = multiview->Multiview->View(i).Static.Geometry->GetRenderObjectCount();
                
                PushConstant pushConstant = {
                    .ObjectCount = meshCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshCount, 1, 1},
                    {64, 1, 1});
            }
        });

    return pass;
}
