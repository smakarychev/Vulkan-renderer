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
        .Meshlets = renderGraph.Import("Meshlets"_hsv, geometry.Meshlets.GetUnderlyingBuffer()),
        .RenderObjects = renderGraph.Import("RenderObjects"_hsv, geometry.RenderObjects.GetUnderlyingBuffer()),
        .Attributes = renderGraph.Import("Attributes"_hsv, geometry.Attributes.GetUnderlyingBuffer()),
        .Indices = renderGraph.Import("Indices"_hsv, geometry.Indices.GetUnderlyingBuffer()),
        .JointMatrices = renderGraph.Import("JointMatrices"_hsv, geometry.JointMatrices.GetUnderlyingBuffer()),
        .Skins = renderGraph.Import("Skins"_hsv, geometry.Skins.GetUnderlyingBuffer()),
        .BlendShapes = renderGraph.Import("BlendShapes"_hsv, geometry.BlendShapes.GetUnderlyingBuffer()),
        .Materials = renderGraph.Import("Materials"_hsv, geometry.Materials.GetUnderlyingBuffer()),
        .RenderObjectSkinnedInfos = 
            renderGraph.Import("RenderObjectSkinnedInfos"_hsv, geometry.RenderObjectSkinnedInfos.GetUnderlyingBuffer()),
        .RenderObjectSkinnedInfoIndices = renderObjectSkinnedInfoIndices,
    };
}
