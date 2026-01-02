#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::AtmosphereLutTransmittanceAtView
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource TransmittanceLut{};
};

struct PassData
{
    RG::Resource ViewInfo{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
