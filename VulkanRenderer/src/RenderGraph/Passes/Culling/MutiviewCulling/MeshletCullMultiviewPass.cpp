#include "MeshletCullMultiviewPass.h"

#include "CullMultiviewResources.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Multiview::MeshletCull::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const MeshletCullMultiviewPassExecutionInfo& info, CullStage stage)
{
    using namespace RG;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Meshlet.Cull.Multiview.Setup")

            graph.SetShader("../assets/shaders/meshlet-cull-multiview.shader",
                ShaderOverrides{
                    ShaderOverride{{"REOCCLUSION"}, stage == CullStage::Reocclusion},
                    ShaderOverride{{"SINGLE_PASS"}, stage == CullStage::Single}});
            
            RgUtils::readWriteCullMeshletMultiview(*info.MultiviewResource, stage, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Meshlet.Cull.Multiview")
            GPU_PROFILE_FRAME("Meshlet.Cull.Multiview")

            auto* multiview = passData.MultiviewResource;

            Sampler hizSampler = multiview->HiZSampler;
            
            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            samplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                multiview->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

            RgUtils::updateCullMeshletMultiviewBindings(resourceDescriptors, resources, *multiview, stage);

            struct PushConstant
            {
                u32 MeshletCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshletCount = multiview->Multiview->View(i).Static.Geometry->GetMeshletCount();
                
                PushConstant pushConstant = {
                    .MeshletCount = meshletCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshletCount, 1, 1},
                    {64, 1, 1});
            }
        });

    return pass;
}
