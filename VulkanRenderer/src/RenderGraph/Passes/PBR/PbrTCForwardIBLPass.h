#pragma once

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"

class Camera;
class SceneGeometry;

struct PbrForwardIBLPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    glm::uvec2 Resolution{};
    const Camera* Camera{};
    RG::Resource ColorIn{};
    RG::Resource DepthIn{};

    const SceneLight* SceneLights{nullptr};
    RG::IBLData IBL{};
};

/* Main forward pbr ibl pass 'TC' stands for triangle culling
 * This pass is suitable to draw opaque surfaces only
 */

namespace Passes::Pbr::ForwardTcIbl
{
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const PbrForwardIBLPassExecutionInfo& info);
}
