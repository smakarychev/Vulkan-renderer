#pragma once

#include "RenderGraph/RGDrawResources.h"

class SceneLight2;
class SceneGeometry2;

namespace Passes::SceneVBufferPbr
{
    struct ExecutionInfo
    {
        const SceneGeometry2* Geometry{nullptr};
        RG::Resource VisibilityTexture{};
        RG::Resource Camera{};
        const SceneLight2* Lights{nullptr};
        RG::SSAOData SSAO{};
        RG::IBLData IBL{};
        RG::Resource Clusters{};
        RG::Resource Tiles{};
        RG::Resource ZBins{};
    };
    struct PassData
    {
        RG::Resource Color{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
