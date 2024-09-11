#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"

class SceneLight;
class SceneGeometry;

struct PbrVisibilityBufferExecutionInfo
{
    RG::Resource VisibilityTexture{};
    RG::Resource ColorIn{};

    const SceneLight* SceneLights{nullptr};
    RG::Resource Clusters{};
    RG::IBLData IBL{};
    RG::SSAOData SSAO{};
    RG::CSMData CSMData{};

    const SceneGeometry* Geometry{nullptr};
};

namespace Passes::Pbr::VisibilityIbl
{
    struct PassData
    {
        RG::Resource VisibilityTexture{};
        RG::SceneLightResources LightsResources{};
        RG::Resource Clusters{};
        RG::IBLData IBL{};
        RG::SSAOData SSAO{};
        RG::CSMData CSMData{};
        
        RG::Resource Camera{};
        RG::Resource ShadingSettings{};
        RG::Resource Commands{};
        RG::Resource Objects{};
        RG::Resource Positions{};
        RG::Resource Normals{};
        RG::Resource Tangents{};
        RG::Resource UVs{};
        RG::Resource Indices{};

        RG::Resource ColorOut{};
    };

    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info);
}
