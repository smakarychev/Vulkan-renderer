#pragma once

#include "CullMultiviewData.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"
#include "RenderGraph/RenderGraph.h"

#include <vector>

struct TriangleDrawContextMultiview;
class ShaderDescriptors;
class CullMultiviewData;

namespace RG
{
    struct CullMultiviewResource
    {
        std::vector<Resource> Objects;
        std::vector<Resource> Meshlets;
        std::vector<Resource> Commands;
        Resource ViewSpans;
        
        Sampler HiZSampler{};
        std::vector<Resource> HiZs;
        std::vector<Resource> Views;
        
        std::vector<Resource> MeshVisibility;
        std::vector<Resource> MeshletVisibility;

        std::vector<Resource> CompactCommands;
        std::vector<Resource> CompactCommandCount;
        std::vector<Resource> CompactCommandCountReocclusion;
        /* is used to mark commands that were processed by triangle culling */
        std::vector<Resource> CommandFlags;

        const std::vector<CullViewDescription>* ViewDescriptions{nullptr};
    };

    struct CullTrianglesMultiviewResource
    {
        std::vector<Resource> BatchDispatches{};
        std::vector<Resource> Indices;
        std::vector<Resource> TriangleVisibility;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Triangles;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulled;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulledCount;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Draw;

        const std::vector<CullViewDescription>* ViewDescriptions{nullptr};
    };
}

namespace RG::RgUtils
{
    CullMultiviewResource createCullMultiview(const CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName);
    void readWriteCullMeshMultiview(CullMultiviewResource& multiview, Graph& graph);
    void updateMeshCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResource& multiview);
    
    void readWriteCullMeshletMultiview(CullMultiviewResource& multiview, CullStage cullStage, bool triangleCull,
        Graph& graph);
    void updateMeshletCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResource& multiview, CullStage cullStage, bool triangleCull,
        ResourceUploader& resourceUploader);
}
