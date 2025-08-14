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
        RG::Resource ColorIn{};
        RG::Resource IrradianceSH{};
        const SceneLight* Light{nullptr};
        const CloudParameters* CloudParameters{nullptr};
        CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
        u32 FaceIndex{0};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource CloudCoverage{};
        RG::Resource CloudProfile{};
        RG::Resource CloudShapeLowFrequencyMap{};
        RG::Resource CloudShapeHighFrequencyMap{};
        RG::Resource CloudCurlNoise{};
        RG::Resource IrradianceSH{};
        RG::Resource DirectionalLights{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);    
}
