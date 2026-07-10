#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Mipmap
{
struct PassData
{
    RG::ImageResource Texture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource image);
}
