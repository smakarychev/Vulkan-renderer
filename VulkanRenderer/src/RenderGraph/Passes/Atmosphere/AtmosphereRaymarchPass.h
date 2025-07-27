#pragma once
#include "RenderGraph/RGResource.h"

class Camera;
class SceneLight;

namespace Passes::Atmosphere::Raymarch
{
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        const SceneLight* Light{nullptr};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspective{};
        RG::Resource Clouds{};
        RG::Resource CloudsDepth{};
        RG::Resource ColorIn{};
        RG::Resource DepthIn{};
        bool UseSunLuminance{false};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource DepthIn{};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspective{};
        RG::Resource Clouds{};
        RG::Resource CloudsDepth{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

