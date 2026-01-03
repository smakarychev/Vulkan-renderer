#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::AerialPerspective
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource TransmittanceLut{};
    RG::Resource MultiscatteringLut{};
    RG::CsmData CsmData{};
};

struct PassData
{
    RG::Resource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
