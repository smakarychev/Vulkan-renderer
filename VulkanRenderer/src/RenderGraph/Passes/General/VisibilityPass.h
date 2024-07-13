#pragma once

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMultiviewData.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"

#include <memory>


struct VisibilityPassInitInfo
{
    const SceneGeometry* Geometry{nullptr};
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    CameraType CameraType{CameraType::Perspective};
};

struct VisibilityPassExecutionInfo
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
};

class VisibilityPass
{
public:
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
        RG::Resource HiZOut{};
    };
public:
    VisibilityPass(RG::Graph& renderGraph, const VisibilityPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const VisibilityPassExecutionInfo& info);
    HiZPassContext* GetHiZContext() const { return m_MultiviewData.View(0).Static.HiZContext.get(); }
private:
    CullMultiviewData m_MultiviewData{};
    std::shared_ptr<CullMetaMultiviewPass> m_Pass{};
};
