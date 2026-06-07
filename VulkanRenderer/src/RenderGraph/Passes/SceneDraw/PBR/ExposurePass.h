#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::PbrCameraExposure
{
struct LuminanceHistogramVisualizationInfo
{
    u32 Width{256};
    u32 Height{128};
    bool AsOverlay{false};
};
struct ExposureSettings
{
    f32 Aperture{14.0};
    f32 ShutterTime{1.0f / 100.0f};
    f32 ISO{100.0f};
    bool UseAutomaticExposure{false};
    bool Visualize{false};
    LuminanceHistogramVisualizationInfo VisualizationInfo;
};

struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource Color{};
    const ExposureSettings* ExposureSettings{nullptr};
};

struct PassData
{
    RG::Resource ViewInfo{};
    RG::Resource HistogramVisualization{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

f32 convertEV100ToExposure(f32 ev100);
}
