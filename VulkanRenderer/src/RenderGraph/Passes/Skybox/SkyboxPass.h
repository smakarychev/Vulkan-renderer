#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::Skybox
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource SkyboxResource{};
    RG::ImageResource Color{};
    RG::ImageResource Depth{};
    glm::uvec2 Resolution{};
    f32 LodBias{0.0f};
};

struct PassData
{
    RG::ImageResource Color{};
    RG::ImageResource Depth{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
