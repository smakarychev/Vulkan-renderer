#pragma once
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

namespace RG
{
    class Geometry;
}

struct DrawIndirectPassInitInfo
{
    RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};
    const ShaderPipeline* DrawPipeline{nullptr};
    std::optional<ShaderDescriptors> MaterialDescriptors{};
};

struct DrawIndirectPassExecutionInfo
{
    RG::Resource Color{};
    RG::Resource Depth{};
    const RG::Geometry* Geometry{nullptr};
    RG::Resource Commands{};
    glm::uvec2 Resolution{};
    AttachmentLoad DepthOnLoad{AttachmentLoad::Load};

    std::optional<RG::IBLData> IBL{};
    std::optional<RG::SSAOData> SSAO{};
};

class DrawIndirectPass
{
public:
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
    };
public:
    DrawIndirectPass(RG::Graph& renderGraph, std::string_view name, const DrawIndirectPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const DrawIndirectPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    struct PassDataPrivate
    {
        RG::Resource CameraUbo{};
        RG::DrawAttributeBuffers AttributeBuffers{};
        RG::Resource ObjectsSsbo{};
        RG::Resource CommandsIndirect{};
        
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};

        std::optional<RG::IBLData> IBL{};
        std::optional<RG::SSAOData> SSAO{};
        RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};

        RG::BindlessTexturesPipelineData* PipelineData{nullptr};
    };
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::DrawFeatures m_Features{RG::DrawFeatures::AllAttributes};
    RG::BindlessTexturesPipelineData m_PipelineData;
};