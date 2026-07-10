#pragma once

#include "ViewInfoGPU.h"
#include "RenderGraph/RGResource.h"

namespace lux
{
struct CommonLight;
}

class Camera;

namespace Passes::Clouds::VP::Shadow
{
struct ExecutionInfo
{
    const Camera* PrimaryCamera{nullptr};
    const ViewInfoGPU* PrimaryView{nullptr};
    RG::ImageResource CloudCoverage{};
    RG::ImageResource CloudProfile{};
    RG::ImageResource CloudShapeLowFrequencyMap{};
    RG::ImageResource CloudShapeHighFrequencyMap{};
    RG::ImageResource CloudCurlNoise{};
    RG::BufferResource CloudParameters{};
    const lux::CommonLight* Light{nullptr};
};

struct PassData
{
    RG::ImageResource Shadow{};
    ViewInfoGPU ShadowView{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
CameraGPU createShadowCamera(const Camera& primaryCamera, const ViewInfoGPU& primaryView,
    const glm::vec3& lightDirection);
}
