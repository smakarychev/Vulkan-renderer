#pragma once

#include "RenderGraph/RenderPass.h"

class ImGuiTexturePass
{
public:
    static void AddToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& texture);
    static void AddToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn);
};
