#pragma once
#include "RenderGraph/RGResource.h"

struct ViewInfoGPU;
class SceneLight;

namespace Passes::Atmosphere::Environment
{
struct ExecutionInfo
{
    const ViewInfoGPU* PrimaryView{nullptr};
    RG::BufferResource PrimaryViewResource{};
    RG::ImageResource SkyViewLut{};
    /* optional external color image resource */
    RG::ImageResource ColorIn{};
    Span<const u32> FaceIndices;
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
