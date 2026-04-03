#include "rendererpch.h"

#include "SceneVisibilityPassesCommon.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/Types/SceneVisibilityElementUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/SceneVisibilityCountDataUniform.generated.h"

SceneVisibilityPassesResources SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
    RG::Graph& renderGraph, const SceneMultiviewVisibility& sceneMultiviewVisibility)
{
    auto& set = sceneMultiviewVisibility.ObjectSet();
    
    SceneVisibilityPassesResources resources = {};
    resources.RenderObjectCount = set.RenderObjectCount();
    resources.MeshletCount = set.MeshletCount();

    resources.Meshlets = renderGraph.Import("Meshlets"_hsv,
        Device::GetBufferArenaUnderlyingBuffer(set.Geometry().Meshlets));
    resources.RenderObjects = renderGraph.Import("RenderObjects"_hsv, set.Geometry().RenderObjects.Buffer);
    resources.RenderObjectBuckets = renderGraph.Import("RenderObjectBuckets"_hsv, set.BucketBits());
    resources.RenderObjectHandles = renderGraph.Import("RenderObjectHandles"_hsv, set.RenderObjectHandles());

    resources.VisibleRenderObjectsData = renderGraph.Create("VisibleRenderObjectsData"_hsv, RG::RGBufferDescription{
        .SizeBytes = resources.RenderObjectCount * sizeof(::gen::SceneVisibilityElement)
    });
    resources.OccludedRenderObjectsData = renderGraph.Create("OccludedRenderObjectsData"_hsv, RG::RGBufferDescription{
        .SizeBytes = resources.RenderObjectCount * sizeof(::gen::SceneVisibilityElement)
    });
    resources.VisibleMeshletsData = renderGraph.Create("VisibleMeshletsData"_hsv, RG::RGBufferDescription{
        .SizeBytes = resources.MeshletCount * sizeof(::gen::SceneVisibilityElement)
    });
    resources.OccludedMeshletsData = renderGraph.Create("OccludedMeshletsData"_hsv, RG::RGBufferDescription{
        .SizeBytes = resources.MeshletCount * sizeof(::gen::SceneVisibilityElement)
    });
    resources.ExpandedMeshlets = renderGraph.Create("ExpandedMeshlets"_hsv, RG::RGBufferDescription{
        .SizeBytes = resources.MeshletCount * sizeof(::gen::SceneVisibilityElement)
    });

    resources.VisibilityCountData = renderGraph.Create("VisibilityCountData"_hsv,
        RG::RGBufferDescription{.SizeBytes = sizeof(::gen::SceneVisibilityCountData)});

    resources.VisibilityCount = sceneMultiviewVisibility.VisibilityCount();

    for (u32 i = 0; i < resources.VisibilityCount; i++)
        resources.Views[i] = renderGraph.Create("View"_hsv, RG::RGBufferDescription{.SizeBytes = sizeof(ViewInfoGPU)});

    const u32 bucketCount = set.BucketCount();
    ASSERT(bucketCount * resources.VisibilityCount < MAX_DRAW_COMMAND_BUFFERS,
        "Too many combinations of views and buckets ({})", bucketCount * resources.VisibilityCount)
    for (u32 view = 0; view < resources.VisibilityCount; view++)
    {
        for (u32 bucket = 0; bucket < bucketCount; bucket++)
        {
            const u32 index = view * bucketCount + bucket;
            
            resources.Draws[index] = renderGraph.Create(StringId("Draws{}_{}", view, bucket),
                RG::RGBufferDescription{.SizeBytes = resources.MeshletCount * sizeof(IndirectDrawCommand)});
            resources.DrawInfos[index] = renderGraph.Create(StringId("DrawsInfo{}_{}", view, bucket),
                RG::RGBufferDescription{.SizeBytes = sizeof(SceneBucketDrawInfo)});
        }
    }
    
    return resources;
}

void SceneVisibilityPassesResources::Init(const SceneMultiviewVisibility& sceneMultiviewVisibility,
    RG::Graph& renderGraph)
{
    VisibilityCountData = renderGraph.Upload(VisibilityCountData, ::gen::SceneVisibilityCountData{});
    for (u32 i = 0; i < VisibilityCount; i++)
    {
        if (sceneMultiviewVisibility.View({i}).ViewInfo.IsViewInfoGPU())
            Views[i] = renderGraph.Upload(Views[i], sceneMultiviewVisibility.View({i}).ViewInfo.AsViewInfoGPU());
        else
            Views[i] = sceneMultiviewVisibility.View({i}).ViewInfo.AsHandle<RG::Resource>();
    }
}