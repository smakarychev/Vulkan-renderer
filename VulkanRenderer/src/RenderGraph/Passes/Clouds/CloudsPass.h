#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Clouds
{
    struct CloudParameters
    {
        f32 CloudMapMetersPerTexel{10.0};
        f32 ShapeNoiseScale = 1.0f / 3000.0f;
        f32 DetailNoiseScaleMultiplier = 8.0f;
        f32 DetailNoiseContribution = 0.1f;
        f32 DetailNoiseHeightModifier = 3.0f;
        f32 WindAngle{glm::radians(35.0f)};
        f32 WindSpeed{1.0f};
        f32 WindUprightAmount{0.1f};
        f32 WindHorizontalSkew{500.0f};
        
        f32 CoverageWindAngle{glm::radians(-135.0f)};
        f32 CoverageWindSpeed{2.0f};
        f32 CoverageWindHorizontalSkew{100.0f};

        glm::vec4 AnvilStratus{0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 AnvilStratocumulus{0.5f, 0.0f, 0.5f, 0.5f};
        glm::vec4 AnvilCumulus{0.5f, 0.5f, 0.5f, 0.6f};

        f32 CurlNoiseScaleMultiplier{0.3f};
        f32 CurlNoiseHeight{0.6f};
        f32 CurlNoiseContribution{0.8f};

        u32 BlueNoiseBindlessIndex{~0u};
        f32 HGEccentricity{0.6f};
        f32 HGBackwardEccentricity{-0.5f};
        f32 HGMixCoefficient{0.5f};
    };
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudMap{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource DepthIn{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource ColorIn{};
        RG::Resource IrradianceSH{};
        const SceneLight* Light{nullptr};
        const CloudParameters* CloudParameters{nullptr};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudMap{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource DepthIn{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource ColorOut{};
        RG::Resource IrradianceSH{};
        RG::Resource DirectionalLights{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);    
}
