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
    struct CullMultiviewResources
    {
        u32 ViewCount{0};
        
        std::vector<Resource> Objects;
        std::vector<Resource> Meshlets;
        std::vector<Resource> Commands;
        
        Resource ViewSpans;
        Resource Views;
        
        Sampler HiZSampler{};
        std::vector<Resource> HiZs;
        
        std::vector<Resource> MeshVisibility;
        std::vector<Resource> MeshletVisibility;

        std::vector<Resource> CompactCommands;
        std::vector<Resource> CompactCommandCount;
        std::vector<Resource> CompactCommandCountReocclusion;
        /* is used to mark commands that were processed by triangle culling */
        std::vector<Resource> CommandFlags;

        const CullMultiviewData* Multiview{nullptr};
    };

    struct CullTrianglesMultiviewResource
    {
        const CullMultiviewResources* CullResources{nullptr};
        u32 ViewCount{0};

        std::vector<Resource> Indices;
        
        Resource MaxDispatches{};
        
        std::vector<Resource> BatchDispatches;
        std::vector<Resource> TriangleVisibility;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Triangles;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulled;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulledCount;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Draw;
    };
}

namespace RG::RgUtils
{
    CullMultiviewResources createCullMultiview(const CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName);
    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph);
    void updateMeshCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview);
    
    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, bool triangleCull,
        Graph& graph);
    void updateMeshletCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage, bool triangleCull,
        ResourceUploader& resourceUploader);

    CullTrianglesMultiviewResource createTriangleCullMultiview(const CullMultiviewResources& multiview, Graph& graph,
        const std::string& baseName);
}
