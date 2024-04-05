#pragma once
#include "DrawResources.h"
#include "..\RGResource.h"
#include "RenderGraph/RenderPass.h"
#include "..\RGCommon.h"

struct DrawIndirectPassInitInfo
{
    RenderGraph::DrawFeatures DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};
    const ShaderPipeline* DrawPipeline{nullptr};
    std::optional<ShaderDescriptors> MaterialDescriptors{};
};

struct DrawIndirectPassExecutionInfo
{
    RenderGraph::Resource Color{};
    RenderGraph::Resource Depth{};
    RenderGraph::Resource Commands{};
    glm::uvec2 Resolution{};
    AttachmentLoad DepthOnLoad{AttachmentLoad::Load};

    std::optional<RenderGraph::IBLData> IBL{};
    std::optional<RenderGraph::SSAOData> SSAO{};
};

class DrawIndirectPass
{
public:
    struct PassData
    {
        RenderGraph::Resource CameraUbo;
        RenderGraph::Resource ObjectsSsbo;
        RenderGraph::Resource CommandsIndirect;
        RenderGraph::Resource ColorOut;
        RenderGraph::Resource DepthOut;

        std::optional<RenderGraph::IBLData> IBL{};
        std::optional<RenderGraph::SSAOData> SSAO{};
        RenderGraph::DrawFeatures DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};

        RenderGraph::BindlessTexturesPipelineData* PipelineData{nullptr};
    };
public:
    DrawIndirectPass(RenderGraph::Graph& renderGraph, std::string_view name, const DrawIndirectPassInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const RenderPassGeometry& geometry,
        const DrawIndirectPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::DrawFeatures m_Features{RenderGraph::DrawFeatures::AllAttributes};
    RenderGraph::BindlessTexturesPipelineData m_PipelineData;
};
