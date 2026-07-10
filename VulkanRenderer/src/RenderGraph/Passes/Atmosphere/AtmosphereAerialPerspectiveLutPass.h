#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::AerialPerspective
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource TransmittanceLut{};
    RG::ImageResource MultiscatteringLut{};
    RG::CsmData CsmData{};
};

struct PassData
{
    RG::ImageResource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
