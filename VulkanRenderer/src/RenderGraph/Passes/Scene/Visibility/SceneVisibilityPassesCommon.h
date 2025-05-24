#pragma once

#include "RenderGraph/RGResource.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

enum class SceneVisibilityStage {Cull, Reocclusion, Single};

struct SceneVisibilityPassesResources
{
    RG::Resource ReferenceCommands{};
    RG::Resource RenderObjects{};
    RG::Resource Meshlets{};
    RG::Resource RenderObjectBuckets{};
    RG::Resource RenderObjectHandles{};
    RG::Resource MeshletHandles{};
    RG::Resource Views{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Hiz{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> MinMaxDepthReductions{};

    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> RenderObjectVisibility{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> MeshletVisibility{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> MeshletBucketInfos{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> MeshletInfoCounts{};
    
    u32 VisibilityCount{0};
    u32 RenderObjectCount{0};
    u32 MeshletCount{0};
    
    static SceneVisibilityPassesResources FromSceneMultiviewVisibility(
        RG::Graph& renderGraph,
        const SceneMultiviewVisibility& sceneMultiviewVisibility);

    void UploadViews(const SceneMultiviewVisibility& sceneMultiviewVisibility,
        RG::Graph& renderGraph);
    void ResetMeshletCounts(RG::Graph& renderGraph);
};