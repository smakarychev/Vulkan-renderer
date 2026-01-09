#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::CopyTexture
{
struct ExecutionInfo
{
    RG::Resource TextureIn{};
    RG::Resource TextureOut{};
    glm::vec3 Offset{0.0f};
    glm::vec3 Size{1.0f};
    ImageSizeType SizeType{ImageSizeType::Relative};
};

struct PassData
{
    RG::Resource TextureIn;
    RG::Resource TextureOut;
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
