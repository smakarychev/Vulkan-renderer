#pragma once
#include "RenderGraph/RenderGraph.h"

enum class SsaoBlurPassKind
{
    Horizontal, Vertical
};

namespace Passes::SsaoBlur
{
    struct PassData
    {
        RG::Resource SsaoIn{};
        RG::Resource SsaoOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource ssao, RG::Resource colorOut,
        SsaoBlurPassKind kind);
}
