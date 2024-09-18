#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesSetup
{
    struct PassData
    {
        RG::Resource Tiles{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph);
}
