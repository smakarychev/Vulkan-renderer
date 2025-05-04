#pragma once

#include "RenderGraph/RenderPass.h"

namespace Passes::ImGuiTexture
{
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiCubeTexture
{
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiTexture3d
{
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
    RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}