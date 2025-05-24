#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::Environment
{
    struct ExecutionInfo
    {
        RG::Resource AtmosphereSettings{};
        const SceneLight* SceneLight{nullptr};
        RG::Resource SkyViewLut{};
    };
    struct PassData
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

