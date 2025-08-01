#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Clouds::VP
{
    enum class CloudsRenderingMode : u8
    {
        FullResolution,
        Reprojection,
    };
    struct CloudParameters
    {
        f32 CloudMapMetersPerTexel{64.0};
        f32 ShapeNoiseScale = 1.0f / 4000.0f;
        f32 DetailNoiseScaleMultiplier = 8.0f;
        f32 DetailNoiseContribution = 0.1f;
        f32 DetailNoiseHeightModifier = 3.0f;
        f32 WindAngle{glm::radians(35.0f)};
        f32 WindSpeed{0.25f};
        f32 WindUprightAmount{0.1f};
        f32 WindHorizontalSkew{500.0f};
        
        glm::vec4 AnvilStratus{0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 AnvilStratocumulus{0.5f, 0.0f, 6.5f, 2.5f};
        glm::vec4 AnvilCumulus{0.5f, 0.5f, 0.5f, 0.6f};

        f32 CurlNoiseScaleMultiplier{0.3f};
        f32 CurlNoiseHeight{0.6f};
        f32 CurlNoiseContribution{0.8f};

        f32 HGEccentricity{0.2f};
        f32 HGBackwardEccentricity{-0.15f};
        f32 HGMixCoefficient{0.67f};
        u32 BlueNoiseBindlessIndex{~0u};
    };
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudCoverage{};
        RG::Resource CloudProfile{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource DepthIn{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource IrradianceSH{};
        const SceneLight* Light{nullptr};
        const CloudParameters* CloudParameters{nullptr};
        CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudCoverage{};
        RG::Resource CloudProfile{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource DepthIn{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource IrradianceSH{};
        RG::Resource DirectionalLights{};
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);    
}
