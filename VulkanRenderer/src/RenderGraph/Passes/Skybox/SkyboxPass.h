#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::Skybox
{
    struct ExecutionInfo
    {
        Texture SkyboxTexture{};
        RG::Resource SkyboxResource{};
        RG::Resource Color{};
        RG::Resource Depth{};
        glm::uvec2 Resolution{};
        f32 LodBias{0.0f};
    };
    struct PassData
    {
        RG::Resource Color{};
        RG::Resource Depth{};
    };
    
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
