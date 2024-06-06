#pragma once

#include <glm/glm.hpp>

#include "RenderGraph/RGDrawResources.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class SceneLight;
class SceneGeometry;

using DrawIndirectCountPassInitInfo = RG::DrawInitInfo;

struct DrawIndirectCountPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    RG::Resource Commands{};
    u32 CommandsOffset{0};
    RG::Resource CommandCount{};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};

    RG::DrawExecutionInfo DrawInfo{};
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
        RG::Resource Camera{};
        RG::DrawAttributeBuffers AttributeBuffers{};
        RG::Resource Objects{};
        RG::Resource Commands{};
        RG::Resource Count{};
        
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