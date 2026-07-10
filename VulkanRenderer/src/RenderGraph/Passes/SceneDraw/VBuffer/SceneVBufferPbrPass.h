#pragma once

#include "RenderGraph/RGDrawResources.h"

struct SceneGeometryRGResources;
class SceneLight;

namespace Passes::SceneVBufferPbr
{
struct ExecutionInfo
{
    const SceneGeometryRGResources* Geometry{nullptr};
    RG::BufferResource VisibleMeshlets{};
    RG::ImageResource VisibilityTexture{};
    RG::BufferResource ViewInfo{};
    RG::BufferResource DirectionalLights{};
    RG::BufferResource PointLights{};
    RG::SSAOData SSAO{};
    RG::IBLData IBL{};
    RG::BufferResource Clusters{};
    RG::BufferResource Tiles{};
    RG::BufferResource ZBins{};
    RG::CsmData CsmData{};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
