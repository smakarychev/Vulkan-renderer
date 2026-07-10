#pragma once

#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::SkyView
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource TransmittanceLut{};
    RG::ImageResource MultiscatteringLut{};
};

struct PassData
{
    RG::ImageResource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
