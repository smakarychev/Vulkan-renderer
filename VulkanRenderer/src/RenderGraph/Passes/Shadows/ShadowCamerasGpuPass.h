#pragma once
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGResource.h"

// todo: finish me! so far the greatest problem is that I did not account for a fact that camera might come from gpu...
namespace Passes::ShadowCamerasGpu
{
    struct PassData
    {
        RG::Resource DepthMinMax;
        RG::Resource PrimaryCamera;
        RG::Resource CsmDataOut;
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depthMinMax,
        RG::Resource primaryCamera, const glm::vec3& lightDirection);
}
