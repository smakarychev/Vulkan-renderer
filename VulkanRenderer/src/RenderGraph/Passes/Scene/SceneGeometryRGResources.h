#pragma once
#include "RenderGraph/RGResource.h"

class SceneGeometry;

struct SceneGeometryRGResources
{
    RG::Resource Meshlets{};
    RG::Resource RenderObjects{};
    RG::Resource Attributes{};
    RG::Resource Indices{};
    RG::Resource JointMatrices{};
    RG::Resource Skins{};
    RG::Resource BlendShapes{};
    RG::Resource Materials{};
    RG::Resource RenderObjectSkinnedInfos{};
    RG::Resource RenderObjectSkinnedInfoIndices{};
    
    static SceneGeometryRGResources ForGeometry(const SceneGeometry& geometry, RG::Graph& renderGraph);
};
