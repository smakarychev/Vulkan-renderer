#pragma once
#include "RenderGraph/RGResource.h"

class Camera;
class SceneLight;

namespace Passes::Atmosphere::Raymarch
{
    struct ExecutionInfo
    {
        RG::Resource AtmosphereSettings{};
        const Camera* Camera{nullptr};
        const SceneLight* Light{nullptr};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspective{};
        RG::Resource ColorIn{};
        RG::Resource DepthIn{};
        bool UseSunLuminance{false};
    };
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspective{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

