#pragma once
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

class SceneLight;
class Camera;

class SceneGeometry;

using DrawIndirectPassInitInfo = RG::DrawInitInfo;

struct DrawIndirectPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    RG::Resource Commands{};
    u32 CommandsOffset{0};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};

    RG::DrawExecutionInfo DrawInfo{};
};

class DrawIndirectPass
{
public:
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};
    };
public:
    DrawIndirectPass(RG::Graph& renderGraph, std::string_view name, const DrawIndirectPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const DrawIndirectPassExecutionInfo& info);
    u64 GetNameHash() const { return m_Name.Hash(); }
private:
    struct PassDataPrivate
    {
        RG::Resource Camera{};
        RG::DrawAttributeBuffers AttributeBuffers{};
        RG::Resource Objects{};
        RG::Resource CommandsIndirect{};

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
