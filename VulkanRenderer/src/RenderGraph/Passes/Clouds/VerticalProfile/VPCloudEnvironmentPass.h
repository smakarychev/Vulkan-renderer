#pragma once

#include "VPCloudPass.h"

struct ViewInfoGPU;

namespace Passes::Clouds::VP::Environment
{
struct ExecutionInfo
{
    const ViewInfoGPU* PrimaryView{nullptr};
    RG::Resource CloudCoverage{};
    RG::Resource CloudProfile{};
    RG::Resource CloudShapeLowFrequencyMap{};
    RG::Resource CloudShapeHighFrequencyMap{};
    RG::Resource CloudCurlNoise{};
    RG::Resource CloudEnvironment{};
    /* optional external color image resource */
    RG::Resource ColorIn{};
    RG::Resource AtmosphereEnvironment{};
    RG::Resource IrradianceSH{};
    RG::Resource CloudParameters{};
    CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
    Span<const u32> FaceIndices;
};

struct PassData
{
    RG::Resource CloudEnvironment{};
    RG::Resource AtmosphereWithCloudsEnvironment{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
