#pragma once

#include "RenderGraph/RGResource.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

struct SceneGeometryRGResources;

enum class SceneVisibilityStage {Cull, Reocclusion, Single};

struct SceneVisibilityPassesResources
{
    RG::BufferResource Meshlets{};
    RG::BufferResource RenderObjects{};
    RG::BufferResource RenderObjectBuckets{};
    RG::BufferResource RenderObjectHandles{};
    std::array<RG::BufferResource, SceneMultiviewVisibility::MAX_VIEWS> Views{};
    std::array<RG::ImageResource, SceneMultiviewVisibility::MAX_VIEWS> Hiz{};
    std::array<RG::ImageResource, SceneMultiviewVisibility::MAX_VIEWS> HizPrevious{};
    std::array<RG::BufferResource, SceneMultiviewVisibility::MAX_VIEWS> MinMaxDepthReductions{};

    static constexpr u32 MAX_DRAW_COMMAND_BUFFERS = MAX_BUCKETS_PER_SET * 2;
    std::array<RG::BufferResource, MAX_DRAW_COMMAND_BUFFERS> Draws{};
    std::array<RG::BufferResource, MAX_DRAW_COMMAND_BUFFERS> DrawInfos{};
    
    RG::BufferResource VisibleRenderObjectsData{};
    RG::BufferResource OccludedRenderObjectsData{};
    RG::BufferResource VisibleMeshletsData{};
    RG::BufferResource OccludedMeshletsData{};
    RG::BufferResource ExpandedMeshlets{};
    RG::BufferResource VisibilityCountData{};
    
    u32 VisibilityCount{0};
    u32 RenderObjectCount{0};
    u32 MeshletCount{0};
    
    static SceneVisibilityPassesResources FromSceneMultiviewVisibility(
        RG::Graph& renderGraph,
        SceneGeometryRGResources& sceneGeometryRGResources,
        const SceneMultiviewVisibility& sceneMultiviewVisibility);

    void Init(const SceneMultiviewVisibility& sceneMultiviewVisibility, RG::Graph& renderGraph);
};