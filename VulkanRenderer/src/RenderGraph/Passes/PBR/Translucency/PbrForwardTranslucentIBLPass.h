#pragma once
#include <memory>

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/Culling/MeshCullPass.h"

class SceneLight;
class MeshletCullTranslucentPass;
class MeshletCullTranslucentContext;
class DrawIndirectPass;
class MeshCullContext;

struct PbrForwardTranslucentIBLPassInitInfo
{
    const SceneGeometry* Geometry{nullptr};
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    CameraType CameraType{CameraType::Perspective};
};

struct PbrForwardTranslucentIBLPassExecutionInfo
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
    RG::Resource ColorIn{};
    RG::Resource DepthIn{};
    
    const SceneLight* SceneLights{nullptr};
    RG::IBLData IBL{};
    
    const HiZPassContext* HiZContext{nullptr};
};

/* Pass that renders translucent geometry after the opaque render pass and skybox pass (if any).
 * The translucent geometry has to be sorted before being submitted to render;
 * this pass does not fully utilize culling, because culling does not respect order (outside of subgroups),
 * therefore only per-meshlet culling w/o any compaction is performed
 */
class PbrForwardTranslucentIBLPass
{
public:
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
public:
    PbrForwardTranslucentIBLPass(RG::Graph& renderGraph, const PbrForwardTranslucentIBLPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const PbrForwardTranslucentIBLPassExecutionInfo& info);
private:
    std::shared_ptr<MeshCullContext> m_MeshContext;
    std::shared_ptr<MeshCullSinglePass> m_MeshCull;
    
    std::shared_ptr<MeshletCullTranslucentContext> m_MeshletContext;
    std::shared_ptr<MeshletCullTranslucentPass> m_MeshletCull;

    std::shared_ptr<DrawIndirectPass> m_Draw;
    
    PassData m_PassData{};
};
