#pragma once

#include "VPCloudPass.h"

struct ViewInfoGPU;

namespace Passes::Clouds::VP::Environment
{
struct ExecutionInfo
{
    const ViewInfoGPU* PrimaryView{nullptr};
    RG::BufferResource PrimaryViewResource{};
    RG::ImageResource CloudCoverage{};
    RG::ImageResource CloudProfile{};
    RG::ImageResource CloudShapeLowFrequencyMap{};
    RG::ImageResource CloudShapeHighFrequencyMap{};
    RG::ImageResource CloudCurlNoise{};
    RG::ImageResource CloudEnvironment{};
    /* optional external color image resource */
    RG::ImageResource ColorIn{};
    RG::ImageResource AtmosphereEnvironment{};
    RG::BufferResource IrradianceSH{};
    RG::BufferResource CloudParameters{};
    CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
    Span<const u32> FaceIndices;
};

struct PassData
{
    RG::ImageResource CloudEnvironment{};
    RG::ImageResource AtmosphereWithCloudsEnvironment{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
