#pragma once
#include "RenderGraph/RGResource.h"

class SceneGeometry;

struct SceneGeometryRGResources
{
    RG::BufferResource Meshlets{};
    RG::BufferResource RenderObjects{};
    RG::BufferResource Attributes{};
    RG::BufferResource Indices{};
    RG::BufferResource JointMatrices{};
    RG::BufferResource Skins{};
    RG::BufferResource BlendShapes{};
    RG::BufferResource Materials{};
    RG::BufferResource RenderObjectSkinnedInfos{};
    RG::BufferResource RenderObjectSkinnedInfoIndices{};
    
    static SceneGeometryRGResources ForGeometry(const SceneGeometry& geometry, RG::Graph& renderGraph);
};
