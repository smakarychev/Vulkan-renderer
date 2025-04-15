#include "SceneFillIndirectDrawPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/SceneFillIndirectDrawsBindGroup.generated.h"
#include "Scene/SceneRenderObjectSet.h"

RG::Pass& Passes::SceneFillIndirectDraw::addToGraph(StringId name, RG::Graph& renderGraph,
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
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw.Setup")

            graph.SetShader("scene-fill-indirect-draws.shader");

            passData.FirstBucket = info.RenderObjectSet->FirstBucket();
            passData.BucketCount = info.RenderObjectSet->BucketCount();

            passData.MeshletCount = info.RenderObjectSet->MeshletCount();

            passData.ReferenceCommands = graph.AddExternal("ReferenceCommands"_hsv,
                info.Geometry->Commands.Buffer);
            passData.ReferenceCommands = graph.Read(passData.ReferenceCommands, Compute | Storage);

            passData.MeshletInfos = graph.Read(info.MeshletInfos, Compute | Storage);
            passData.MeshletInfoCount = graph.Read(info.MeshletInfoCount, Compute | Uniform);

            for (auto& pass : info.RenderObjectSet->Passes())
            {
                for (SceneBucketHandle bucketHandle : pass.BucketHandles())
                {
                    const u32 bucketIndex = info.RenderObjectSet->BucketHandleToIndex(bucketHandle);
                    
                    auto& bucket = pass.BucketFromHandle(bucketHandle);
                    ASSERT(!passData.Draws[bucketIndex].IsValid(), "Ambiguous bucket")
                    
                    passData.Draws[bucketIndex] = graph.AddExternal(
                        StringId("Draw"_hsv).AddVersion(bucketIndex),
                        bucket.Draws());
                    passData.Draws[bucketIndex] = graph.Write(passData.Draws[bucketIndex], Compute | Storage);
                    
                    passData.DrawInfos[bucketIndex] = graph.AddExternal(
                        StringId("DrawInfo"_hsv).AddVersion(bucketIndex),
                        bucket.DrawInfo());
                    passData.DrawInfos[bucketIndex] = graph.Read(
                        passData.DrawInfos[bucketIndex], Compute | Storage);
                    passData.DrawInfos[bucketIndex] = graph.Write(
                        passData.DrawInfos[bucketIndex], Compute | Storage);
                    graph.Upload(passData.DrawInfos[bucketIndex], SceneBucketDrawInfo{});
                }
            }
            
            PassData passDataPublic = {};
            passDataPublic.Draws = passData.Draws;
            passDataPublic.DrawInfos = passData.DrawInfos;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")
            GPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")

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
