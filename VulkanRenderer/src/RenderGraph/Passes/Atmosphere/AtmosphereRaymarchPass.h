#pragma once
#include "RenderGraph/RGResource.h"

class Camera;
class SceneLight;

namespace Passes::Atmosphere::Raymarch
{
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const Camera& camera, const SceneLight& light,
        RG::Resource skyViewLut, RG::Resource transmittanceLut, RG::Resource aerialPerspectiveLut,
        RG::Resource colorIn, const ImageSubresourceDescription& colorSubresource, RG::Resource depthIn,
        bool useSunLuminance);
}

