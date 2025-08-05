#pragma once
#include "RenderGraph/RGResource.h"

struct ViewInfoGPU;
class SceneLight;

namespace Passes::Atmosphere::Environment
{
    struct ExecutionInfo
    {
        const ViewInfoGPU* PrimaryView{nullptr};
        const SceneLight* Light{nullptr};
        RG::Resource SkyViewLut{};
        /* optional external color image resource */
        RG::Resource ColorIn{};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource SkyViewLut{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

