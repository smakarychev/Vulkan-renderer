#pragma once
#include "RenderGraph/RGResource.h"

struct ViewInfoGPU;
class SceneLight;

namespace Passes::Atmosphere::Environment
{
struct ExecutionInfo
{
    const ViewInfoGPU* PrimaryView{nullptr};
    RG::Resource SkyViewLut{};
    /* optional external color image resource */
    RG::Resource ColorIn{};
    Span<const u32> FaceIndices;
};

struct PassData
{
    RG::Resource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
