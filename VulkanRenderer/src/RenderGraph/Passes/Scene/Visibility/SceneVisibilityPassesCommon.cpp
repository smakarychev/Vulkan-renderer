#include "rendererpch.h"

#include "SceneVisibilityPassesCommon.h"

#include "RenderGraph/RGGraph.h"
#include "Scene/SceneRenderObjectSet.h"

SceneVisibilityPassesResources SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
    RG::Graph& renderGraph, const SceneMultiviewVisibility& sceneMultiviewVisibility)
{
    auto& set = sceneMultiviewVisibility.ObjectSet();
    
    SceneVisibilityPassesResources resources = {};
    resources.RenderObjectCount = set.RenderObjectCount();
    resources.MeshletCount = set.MeshletCount();

    resources.ReferenceCommands = renderGraph.Import("RederenceCommands"_hsv, set.Geometry().Commands.Buffer);
    resources.RenderObjects = renderGraph.Import("RenderObjects"_hsv, set.Geometry().RenderObjects.Buffer);
    resources.Meshlets = renderGraph.Import("Meshlets"_hsv, set.Geometry().Meshlets.Buffer);
    resources.RenderObjectBuckets = renderGraph.Import("RenderObjectBuckets"_hsv, set.BucketBits());
    resources.RenderObjectHandles = renderGraph.Import("RenderObjectHandles"_hsv, set.RenderObjectHandles());
    resources.MeshletHandles = renderGraph.Import("MeshletHandles"_hsv, set.MeshletHandles());

    resources.VisibilityCount = sceneMultiviewVisibility.VisibilityCount();

    for (u32 i = 0; i < resources.VisibilityCount; i++)
    {
        if (resources.RenderObjectVisibility[i].IsValid())
            continue;

        resources.Views[i] = renderGraph.Create("View"_hsv, RG::RGBufferDescription{.SizeBytes = sizeof(ViewInfoGPU)});
        
        resources.RenderObjectVisibility[i] = renderGraph.Import(StringId("Visibility.{}", i),
            sceneMultiviewVisibility.RenderObjectVisibility({i}));
        resources.MeshletVisibility[i] = renderGraph.Import(StringId("MeshletVisibility.{}", i),
            sceneMultiviewVisibility.MeshletVisibility({i}));
        resources.MeshletBucketInfos[i] = renderGraph.Create(StringId("MeshletInfos.{}", i),
            RG::RGBufferDescription{.SizeBytes = sizeof(SceneMeshletBucketInfo) * resources.MeshletCount});
        resources.MeshletInfoCounts[i] = renderGraph.Create(StringId("MeshletInfoCounts.{}", i),
            RG::RGBufferDescription{.SizeBytes = sizeof(u32)});
    }
    
    return resources;
}

void SceneVisibilityPassesResources::InitViews(const SceneMultiviewVisibility& sceneMultiviewVisibility,
    RG::Graph& renderGraph)
{
    for (u32 i = 0; i < VisibilityCount; i++)
    {
        if (sceneMultiviewVisibility.View({i}).ViewInfo.IsViewInfoGPU())
            Views[i] = renderGraph.Upload(Views[i], sceneMultiviewVisibility.View({i}).ViewInfo.AsViewInfoGPU());
        else
            Views[i] = sceneMultiviewVisibility.View({i}).ViewInfo.AsHandle<RG::Resource>();
    }
}

void SceneVisibilityPassesResources::ResetMeshletCounts(RG::Graph& renderGraph)
{
    for (u32 i = 0; i < VisibilityCount; i++)
        MeshletInfoCounts[i] = renderGraph.Upload(MeshletInfoCounts[i], 0lu);
}
