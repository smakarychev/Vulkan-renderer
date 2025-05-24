#pragma once

#include "RenderGraph/RGDrawResources.h"

class SceneLight;
class SceneGeometry;

namespace Passes::SceneVBufferPbr
{
    struct ExecutionInfo
    {
        const SceneGeometry* Geometry{nullptr};
        RG::Resource VisibilityTexture{};
        RG::Resource Camera{};
        const SceneLight* Lights{nullptr};
        RG::SSAOData SSAO{};
        RG::IBLData IBL{};
        RG::Resource Clusters{};
        RG::Resource Tiles{};
        RG::Resource ZBins{};
        RG::CsmData CsmData{};
    };
    struct PassData
    {
        RG::Resource Color{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
