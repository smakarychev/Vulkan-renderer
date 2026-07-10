#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::PbrTonemapping
{
enum class TonemappingType : u8
{
    Reinhard,
    ReinhardLuminance,
    Hable,
    PbrNeutral,
    Agx,
    GT7,
    
    MaxValue,
};

struct ExecutionInfo
{
    RG::ImageResource Color{};
    TonemappingType Type{TonemappingType::GT7};
};
struct PassData
{
    RG::ImageResource Color;
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

std::string_view tonemappingTypeToString(TonemappingType type);
}
