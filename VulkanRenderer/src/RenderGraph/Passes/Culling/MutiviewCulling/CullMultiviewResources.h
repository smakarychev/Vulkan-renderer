#pragma once

#include "CullMultiviewData.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"
#include "RenderGraph/RenderGraph.h"

#include <vector>

#include "CullMultiviewUtils.h"

struct TriangleDrawContextMultiview;
class ShaderDescriptors;
class CullMultiviewData;

namespace RG
{
    struct CullMultiviewResources
    {
        u32 ViewCount{0};
        u32 GeometryCount{0};
        
        Resource ViewSpans;
        Resource Views;
        Sampler HiZSampler{};
        
        std::vector<Resource> Objects;
        std::vector<Resource> Meshlets;
        std::vector<Resource> Commands;
        
        std::vector<Resource> HiZs;
        
        std::vector<Resource> MeshVisibility;
        std::vector<Resource> MeshletVisibility;

        std::vector<Resource> CompactCommands;
        Resource CompactCommandCount;
        Resource CompactCommandCountReocclusion;

        CullMultiviewData* Multiview{nullptr};
    };

    struct CullTrianglesMultiviewResource
    {
        u32 TriangleViewCount{0};
        std::vector<u32> MeshletViewIndices;

        Resource ViewSpans;
        Resource Views;

        Resource MaxDispatches{};
        
        std::vector<Resource> Indices;
        std::vector<Resource> BatchDispatches;
        
        std::vector<Resource> TriangleVisibility;
        std::vector<std::array<Resource, BATCH_OVERLAP>> Triangles;
        std::vector<std::array<Resource, BATCH_OVERLAP>> IndicesCulled;
        std::array<Resource, BATCH_OVERLAP> IndicesCulledCount;
        std::array<Resource, BATCH_OVERLAP> Draws;

        std::vector<DrawAttributeBuffers> AttributeBuffers;
        std::vector<Resource> Cameras;
        std::vector<SceneLightResources> SceneLights;
        std::vector<DrawAttachmentResources> AttachmentResources;
        std::vector<IBLData> IBLs;
        std::vector<SSAOData> SSAOs;
        std::vector<CSMData> CSMs;
        
        CullMultiviewResources* CullResources{nullptr};
        CullMultiviewData* Multiview{nullptr};
        utils::AttachmentsRenames* AttachmentsRenames{nullptr};
    };
}

namespace RG::RgUtils
{
    CullMultiviewResources createCullMultiview(CullMultiviewData& cullMultiviewData, Graph& graph,
        const std::string& baseName);
    void readWriteCullMeshMultiview(CullMultiviewResources& multiview, Graph& graph);
    void updateMeshCullMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview);
    
    void readWriteCullMeshletMultiview(CullMultiviewResources& multiview, CullStage cullStage, Graph& graph);
    void updateCullMeshletMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullMultiviewResources& multiview, CullStage cullStage);

    CullTrianglesMultiviewResource createTriangleCullMultiview(CullMultiviewResources& multiview, Graph& graph,
        const std::string& baseName);

    void readWriteCullTrianglePrepareMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph);
    void updateCullTrianglePrepareMultiviewBindings(const ShaderDescriptors& descriptors, const Resources& resources,
        const CullTrianglesMultiviewResource& multiview);

    void readWriteCullTriangleMultiview(CullTrianglesMultiviewResource& multiview, Graph& graph);
    void updateCullTriangleMultiviewBindings(const ShaderDescriptors& cullDescriptors,
        const ShaderDescriptors& prepareDescriptors,
        const std::vector<ShaderDescriptors>& drawDescriptors,
        const Resources& resources,
        const CullTrianglesMultiviewResource& multiview,
        u32 batchIndex);
}
