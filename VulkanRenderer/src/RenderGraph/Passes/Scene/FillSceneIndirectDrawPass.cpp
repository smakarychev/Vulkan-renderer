#include "FillSceneIndirectDrawPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/SceneFillIndirectDrawsBindGroup.generated.h"
#include "Scene/SceneRenderObjectSet.h"

RG::Pass& Passes::FillSceneIndirectDraw::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource ReferenceCommands{};
        Resource MeshletInfos{};
        Resource MeshletInfoCount{};

        std::array<Resource, MAX_BUCKETS_PER_SET> Draws;
        std::array<Resource, MAX_BUCKETS_PER_SET> DrawInfos;
        
        SceneBucketHandle FirstBucket{0};
        u32 BucketCount{0};
        
        u32 MeshletCount{0};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.FillSceneIndirectDraw.Setup")

            graph.SetShader("scene-fill-indirect-draws.shader");

            passData.FirstBucket = info.RenderObjectSet->FirstBucket();
            passData.BucketCount = info.RenderObjectSet->BucketCount();

            passData.MeshletCount = info.RenderObjectSet->MeshletCount();

            passData.ReferenceCommands = graph.AddExternal(std::format("{}.ReferenceCommands", name),
                info.Geometry->Commands.Buffer);
            passData.ReferenceCommands = graph.Read(passData.ReferenceCommands, Compute | Storage);

            passData.MeshletInfos = graph.Read(info.MeshletInfos, Compute | Storage);
            passData.MeshletInfoCount = graph.Read(info.MeshletInfoCount, Compute | Uniform);

            u32 currentBucket = 0;
            for (auto& pass : info.RenderObjectSet->Passes())
            {
                for (u32 bucketIndex = 0; bucketIndex < pass.BucketCount(); bucketIndex++)
                {
                    auto& bucket = pass.Bucket(bucketIndex);
                    ASSERT(!passData.Draws[currentBucket].IsValid(), "Ambiguous bucket")
                    
                    passData.Draws[currentBucket] = graph.AddExternal(
                        std::format("{}.Draws.{}", name, currentBucket),
                        bucket.Draws());
                    passData.Draws[currentBucket] = graph.Write(passData.Draws[currentBucket], Compute | Storage);
                    
                    passData.DrawInfos[currentBucket] = graph.AddExternal(
                        std::format("{}.DrawInfos.{}", name, currentBucket),
                        bucket.DrawInfos());
                    passData.DrawInfos[currentBucket] = graph.Read(
                        passData.DrawInfos[currentBucket], Compute | Storage);
                    passData.DrawInfos[currentBucket] = graph.Write(
                        passData.DrawInfos[currentBucket], Compute | Storage);
                    graph.Upload(passData.DrawInfos[currentBucket], SceneBucketDrawInfo{});
                    
                    currentBucket++;
                }
            }
            
            PassData passDataPublic = {};
            passDataPublic.Draws = passData.Draws;
            passDataPublic.DrawInfos = passData.DrawInfos;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.FillSceneIndirectDraw")
            GPU_PROFILE_FRAME("Scene.FillSceneIndirectDraw")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneFillIndirectDrawsShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands({.Buffer = resources.GetBuffer(passData.ReferenceCommands)});
            bindGroup.SetMeshletInfos({.Buffer = resources.GetBuffer(passData.MeshletInfos)});
            bindGroup.SetMeshletInfoCount({.Buffer = resources.GetBuffer(passData.MeshletInfoCount)});
            for (u32 bucketIndex = 0; bucketIndex < passData.BucketCount; bucketIndex++)
            {
                bindGroup.SetDrawCommands({.Buffer = resources.GetBuffer(passData.Draws[bucketIndex])}, bucketIndex);
                bindGroup.SetDrawInfo({.Buffer = resources.GetBuffer(passData.DrawInfos[bucketIndex])}, bucketIndex);
            }

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            /* todo: this can use indirect dispatch */
            cmd.Dispatch({
               .Invocations = {passData.MeshletCount, 1, 1},
               .GroupSize = {256, 1, 1}});
        });
}
