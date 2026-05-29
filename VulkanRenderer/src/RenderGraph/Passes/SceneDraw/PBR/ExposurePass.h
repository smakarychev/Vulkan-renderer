#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::PbrCameraExposure
{
struct ExposureSettings
{
    f32 Aperture{14.0};
    f32 ShutterTime{1.0f / 125.0f};
    f32 ISO{100.0f};
};
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    const ExposureSettings* ExposureSettings{nullptr};
    // todo: calculate from frame luminance
};

struct PassData
{
    RG::Resource ViewInfo{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
