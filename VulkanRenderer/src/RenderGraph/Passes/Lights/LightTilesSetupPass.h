#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesSetup
{
    struct PassData
    {
        RG::Resource Tiles{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph);
}
