#pragma once

#include "ViewInfoGPU.h"
#include "RenderGraph/RGResource.h"

struct CommonLight;
class Camera;

namespace Passes::Clouds::VP
{
    struct CloudParameters;
}

namespace Passes::Clouds::VP::Shadow
{
    struct ExecutionInfo
    {
        const Camera* PrimaryCamera{nullptr};
        const ViewInfoGPU* PrimaryView{nullptr};
        RG::Resource CloudCoverage{};
        RG::Resource CloudProfile{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        const CommonLight* Light{nullptr};
        const CloudParameters* CloudParameters{nullptr};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudCoverage{};
        RG::Resource CloudProfile{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource DepthOut{};
        ViewInfoGPU ShadowView{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
    CameraGPU createShadowCamera(const Camera& primaryCamera, const ViewInfoGPU& primaryView,
        const glm::vec3& lightDirection);
}
