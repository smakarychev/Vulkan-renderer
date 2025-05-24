#pragma once
#include "RenderGraph/RGGraph.h"

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
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
