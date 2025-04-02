#pragma once

#include "RenderGraph/RenderPass.h"

namespace Passes::ImGuiTexture
{
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiCubeTexture
{
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiTexture3d
{
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}