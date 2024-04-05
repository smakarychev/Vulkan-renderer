#pragma once

#include <glm/glm.hpp>

#include "DrawResources.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "..\RGCommon.h"

struct DrawIndirectCountPassInitInfo
{
    RenderGraph::DrawFeatures DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};
    const ShaderPipeline* DrawPipeline{nullptr};
    std::optional<ShaderDescriptors> MaterialDescriptors{};
};

struct DrawIndirectCountPassExecutionInfo
{
    RenderGraph::Resource Color{};
    RenderGraph::Resource Depth{};
    RenderGraph::Resource Commands{};
    RenderGraph::Resource CommandCount{};
    glm::uvec2 Resolution{};
    AttachmentLoad DepthOnLoad{AttachmentLoad::Load};

    std::optional<RenderGraph::IBLData> IBL{};
    std::optional<RenderGraph::SSAOData> SSAO{};
};

class DrawIndirectCountPass
{
public:
    struct PassData
    {
        RenderGraph::Resource CameraUbo{};
        RenderGraph::Resource ObjectsSsbo{};
        RenderGraph::Resource CommandsIndirect{};
        RenderGraph::Resource CountIndirect{};
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource DepthOut{};

        std::optional<RenderGraph::IBLData> IBL{};
        std::optional<RenderGraph::SSAOData> SSAO{};
        RenderGraph::DrawFeatures DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};

        RenderGraph::BindlessTexturesPipelineData* PipelineData{nullptr};
    };
public:
    DrawIndirectCountPass(RenderGraph::Graph& renderGraph, std::string_view name, 
        const DrawIndirectCountPassInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const RenderPassGeometry& geometry,
        const DrawIndirectCountPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::DrawFeatures m_Features{RenderGraph::DrawFeatures::AllAttributes};
    RenderGraph::BindlessTexturesPipelineData m_PipelineData;
};