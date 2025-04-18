#include "SceneVisibilityPassesCommon.h"

#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneRenderObjectSet.h"

SceneVisibilityPassesResources SceneVisibilityPassesResources::FromSceneMultiviewVisibility(
    RG::Graph& renderGraph,
    const SceneMultiviewVisibility& sceneMultiviewVisibility)
{
    auto& set = sceneMultiviewVisibility.ObjectSet();
    
    SceneVisibilityPassesResources resources = {};
    resources.VisibilityCount = sceneMultiviewVisibility.VisibilityCount();
    resources.RenderObjectCount = set.RenderObjectCount();
    resources.MeshletCount = set.MeshletCount();

    resources.ReferenceCommands = renderGraph.AddExternal("RederenceCommands"_hsv, set.Geometry().Commands.Buffer);
    resources.RenderObjects = renderGraph.AddExternal("RenderObjects"_hsv, set.Geometry().RenderObjects.Buffer);
    resources.Meshlets = renderGraph.AddExternal("Meshlets"_hsv, set.Geometry().Meshlets.Buffer);
    resources.RenderObjectBuckets = renderGraph.AddExternal("RenderObjectBuckets"_hsv, set.BucketBits());
    resources.RenderObjectHandles = renderGraph.AddExternal("RenderObjectHandles"_hsv, set.RenderObjectHandles());
    resources.MeshletHandles = renderGraph.AddExternal("MeshletHandles"_hsv, set.MeshletHandles());
    resources.Views = renderGraph.CreateResource("View"_hsv, RG::GraphBufferDescription{
        .SizeBytes = sizeof(SceneViewGPU) * resources.VisibilityCount});

    for (u32 i = 0; i < resources.VisibilityCount; i++)
    {
        resources.RenderObjectVisibility[i] = renderGraph.AddExternal(StringId("Visibility.{}", i),
            sceneMultiviewVisibility.RenderObjectVisibility({i}));
        resources.MeshletVisibility[i] = renderGraph.AddExternal(StringId("MeshletVisibility.{}", i),
            sceneMultiviewVisibility.MeshletVisibility({i}));
        resources.MeshletBucketInfos[i] = renderGraph.CreateResource(StringId("MeshletInfos.{}", i),
            RG::GraphBufferDescription{.SizeBytes = sizeof(SceneMeshletBucketInfo) * resources.MeshletCount});
        resources.MeshletInfoCounts[i] = renderGraph.CreateResource(StringId("MeshletInfoCounts.{}", i),
            RG::GraphBufferDescription{.SizeBytes = sizeof(u32)});
    }
    
    return resources;
}

void SceneVisibilityPassesResources::UploadViews(const SceneMultiviewVisibility& sceneMultiviewVisibility,
    RG::Graph& renderGraph)
{
    for (u32 i = 0; i < VisibilityCount; i++)
        renderGraph.Upload(Views, SceneViewGPU::FromSceneView(sceneMultiviewVisibility.View({i})),
            i * sizeof(SceneViewGPU));
}

void SceneVisibilityPassesResources::ResetMeshletCounts(RG::Graph& renderGraph)
{
    for (u32 i = 0; i < VisibilityCount; i++)
        renderGraph.Upload(MeshletInfoCounts[i], 0lu);
}
