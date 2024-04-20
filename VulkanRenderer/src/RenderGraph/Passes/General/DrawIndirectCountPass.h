#pragma once

#include <glm/glm.hpp>

#include "RenderGraph/RGDrawResources.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class SceneLight;

namespace RG
{
    class Geometry;
}

struct DrawIndirectCountPassInitInfo
{
    RG::DrawFeatures DrawFeatures{RG::DrawFeatures::AllAttributes};
    ShaderPipeline DrawPipeline{};
    std::optional<const ShaderDescriptors*> MaterialDescriptors{};
};

struct DrawIndirectCountPassExecutionInfo
{
    const RG::Geometry* Geometry{nullptr};
    RG::Resource Commands{};
    u32 CommandsOffset{0};
    RG::Resource CommandCount{};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};

    RG::DrawAttachments DrawAttachments{};
    const SceneLight* SceneLights{nullptr};
    std::optional<RG::IBLData> IBL{};
    std::optional<RG::SSAOData> SSAO{};
};

class DrawIndirectCountPass
{
public:
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};
    };
public:
    DrawIndirectCountPass(RG::Graph& renderGraph, std::string_view name, 
        const DrawIndirectCountPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const DrawIndirectCountPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    struct PassDataPrivate
    {
        RG::Resource CameraUbo{};
        RG::DrawAttributeBuffers AttributeBuffers{};
        RG::Resource ObjectsSsbo{};
        RG::Resource CommandsIndirect{};
        RG::Resource CountIndirect{};
        
        RG::DrawAttachmentResources DrawAttachmentResources{};

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