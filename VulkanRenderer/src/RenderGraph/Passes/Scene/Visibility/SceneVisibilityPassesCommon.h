#pragma once

#include "RenderGraph/RGResource.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

enum class SceneVisibilityStage {Cull, Reocclusion, Single};

struct SceneVisibilityPassesResources
{
    RG::Resource Meshlets{};
    RG::Resource RenderObjects{};
    RG::Resource RenderObjectBuckets{};
    RG::Resource RenderObjectHandles{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Views{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Hiz{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> HizPrevious{};
    std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> MinMaxDepthReductions{};

    static constexpr u32 MAX_DRAW_COMMAND_BUFFERS = MAX_BUCKETS_PER_SET * 2;
    std::array<RG::Resource, MAX_DRAW_COMMAND_BUFFERS> Draws{};
    std::array<RG::Resource, MAX_DRAW_COMMAND_BUFFERS> DrawInfos{};
    
    RG::Resource VisibleRenderObjectsData{};
    RG::Resource OccludedRenderObjectsData{};
    RG::Resource VisibleMeshletsData{};
    RG::Resource OccludedMeshletsData{};
    RG::Resource ExpandedMeshlets{};
    RG::Resource VisibilityCountData{};
    
    u32 VisibilityCount{0};
    u32 RenderObjectCount{0};
    u32 MeshletCount{0};
    
    static SceneVisibilityPassesResources FromSceneMultiviewVisibility(
        RG::Graph& renderGraph,
        const SceneMultiviewVisibility& sceneMultiviewVisibility);

    void Init(const SceneMultiviewVisibility& sceneMultiviewVisibility, RG::Graph& renderGraph);
};