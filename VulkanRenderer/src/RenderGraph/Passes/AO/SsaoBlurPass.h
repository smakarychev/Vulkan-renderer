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
    RG::ImageResource SsaoIn{};
    RG::ImageResource SsaoOut{};
    SsaoBlurPassKind BlurKind{SsaoBlurPassKind::Horizontal};
};

struct PassData
{
    RG::ImageResource Ssao{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
