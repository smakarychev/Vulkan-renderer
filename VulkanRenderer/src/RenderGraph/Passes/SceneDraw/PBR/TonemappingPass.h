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
    
    MaxValue,
};

struct ExecutionInfo
{
    RG::Resource Color{};
    TonemappingType Type{TonemappingType::Agx};
};
struct PassData
{
    RG::Resource Color;
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

std::string_view tonemappingTypeToString(TonemappingType type);
}
