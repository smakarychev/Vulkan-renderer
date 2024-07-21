#pragma once

#include "RenderGraph/RenderPass.h"

namespace Passes::ImGuiTexture
{
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& texture);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn);
}