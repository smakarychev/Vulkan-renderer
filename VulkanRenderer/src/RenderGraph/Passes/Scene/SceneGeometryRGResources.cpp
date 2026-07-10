#include "rendererpch.h"
#include "SceneGeometryRGResources.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Utility/UploadPass.h"
#include "Scene/SceneGeometry.h"

SceneGeometryRGResources SceneGeometryRGResources::ForGeometry(const SceneGeometry& geometry, RG::Graph& renderGraph)
{
    const RG::BufferResource renderObjectSkinnedInfoIndices = geometry.RenderObjectSkinnedInfosIndices.empty() ?
        RG::BufferResource{} :
        Passes::Upload::addToGraph("UploadRenderObjectSkinnedInfoIndices"_hsv, 
            renderGraph, geometry.RenderObjectSkinnedInfosIndices);
    
    return {
        .Meshlets = renderGraph.Import("Meshlets"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.Meshlets)),
        .RenderObjects = 
            renderGraph.Import("RenderObjects"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.RenderObjects)),
        .Attributes = 
            renderGraph.Import("Attributes"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.Attributes)),
        .Indices = 
            renderGraph.Import("Indices"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.Indices)),
        .JointMatrices = 
            renderGraph.Import("JointMatrices"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.JointMatrices)),
        .Skins = 
            renderGraph.Import("Skins"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.Skins)),
        .BlendShapes = 
            renderGraph.Import("BlendShapes"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.BlendShapes)),
        .Materials = 
            renderGraph.Import("Materials"_hsv, Device::GetBufferArenaUnderlyingBuffer(geometry.Materials)),
        .RenderObjectSkinnedInfos = 
            renderGraph.Import("RenderObjectSkinnedInfos"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(geometry.RenderObjectSkinnedInfos)),
        .RenderObjectSkinnedInfoIndices = renderObjectSkinnedInfoIndices,
    };
}
