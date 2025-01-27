#include "MeshletCullMultiviewPass.h"

#include "CullMultiviewResources.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/MeshletCullMultiviewBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
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

            graph.SetShader("meshlet-cull-multiview.shader",
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
            MeshletCullMultiviewShaderBindGroup bindGroup(shader);

            bindGroup.SetSampler(resources.GetTexture(
                multiview->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));
            RgUtils::updateCullMeshletMultiviewBindings(bindGroup, resources, *multiview, stage);

            struct PushConstant
            {
                u32 MeshletCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());

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
