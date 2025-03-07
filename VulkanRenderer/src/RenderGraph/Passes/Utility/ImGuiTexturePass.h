#pragma once

#include "RenderGraph/RenderPass.h"

namespace Passes::ImGuiTexture
{
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiCubeTexture
{
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiTexture3d
{
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn);
}