#pragma once

#include "RenderGraph/RGDrawResources.h"

class Camera;
class SceneGeometry;
class SceneLight;
class MeshletCullTranslucentPass;
class MeshletCullTranslucentContext;
class DrawIndirectPass;
class MeshCullContext;

struct PbrForwardTranslucentIBLPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    RG::Resource ColorIn{};
    RG::Resource DepthIn{};
    
    const SceneLight* SceneLights{nullptr};
    RG::IBLData IBL{};
};

/* Pass that renders translucent geometry after the opaque render pass and skybox pass (if any).
 * The translucent geometry has to be sorted before being submitted to render;
 * this pass does not fully utilize culling, because culling does not respect order (outside of subgroups),
 * therefore only per-meshlet culling w/o any compaction is performed
 */
namespace Passes::Pbr::ForwardTranslucentIbl
{
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        const PbrForwardTranslucentIBLPassExecutionInfo& info);
}
