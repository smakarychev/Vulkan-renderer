#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesSetup
{
    struct PassData
    {
        RG::Resource Tiles{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph);
}
