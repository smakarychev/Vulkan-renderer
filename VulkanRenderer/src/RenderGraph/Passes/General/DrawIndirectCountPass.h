#pragma once

#include <glm/glm.hpp>

#include "RenderGraph/RGDrawResources.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

namespace RG
{
    class Geometry;
}

struct DrawIndirectCountPassInitInfo
{
    RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};
    const ShaderPipeline* DrawPipeline{nullptr};
    std::optional<ShaderDescriptors> MaterialDescriptors{};
};

struct DrawIndirectCountPassExecutionInfo
{
    RG::Resource Color{};
    RG::Resource Depth{};
    RG::Resource Commands{};
    RG::Resource CommandCount{};
    glm::uvec2 Resolution{};
    AttachmentLoad DepthOnLoad{AttachmentLoad::Load};

    std::optional<RG::IBLData> IBL{};
    std::optional<RG::SSAOData> SSAO{};
};

class DrawIndirectCountPass
{
public:
    struct PassData
    {
        RG::Resource CameraUbo{};
        RG::Resource ObjectsSsbo{};
        RG::Resource CommandsIndirect{};
        RG::Resource CountIndirect{};
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};

        std::optional<RG::IBLData> IBL{};
        std::optional<RG::SSAOData> SSAO{};
        RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};

        RG::BindlessTexturesPipelineData* PipelineData{nullptr};
    };
public:
    DrawIndirectCountPass(RG::Graph& renderGraph, std::string_view name, 
        const DrawIndirectCountPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const RG::Geometry& geometry,
        const DrawIndirectCountPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::DrawFeatures m_Features{RG::DrawFeatures::AllAttributes};
    RG::BindlessTexturesPipelineData m_PipelineData;
};