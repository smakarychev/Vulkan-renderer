#pragma once
#include "RenderGraph/RenderGraph.h"

enum class SsaoBlurPassKind
{
    Horizontal, Vertical
};

namespace Passes::SsaoBlur
{
    struct ExecutionInfo
    {
        RG::Resource SsaoIn{};
        RG::Resource SsaoOut{};
        SsaoBlurPassKind BlurKind{SsaoBlurPassKind::Horizontal};
    };
    struct PassData
    {
        RG::Resource SsaoIn{};
        RG::Resource SsaoOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
