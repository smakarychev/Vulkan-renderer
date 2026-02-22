#pragma once

#include "ShaderReflection.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include <array>

class RenderCommandList;
struct PushConstantDescription;
class DescriptorLayoutCache;

struct ShaderPipelineTemplateCreateInfo
{
    ShaderReflection ShaderReflection;
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> DescriptorLayoutOverrides{};
};

class ShaderPipelineTemplate
{
public:
    ShaderPipelineTemplate() = default;
    ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo&& createInfo);
    
    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    std::array<bool, MAX_DESCRIPTOR_SETS> GetSetPresence() const;

    bool IsComputeTemplate() const;

    const ShaderReflection& GetReflection() const { return m_ShaderReflection; }

    Span<const ShaderStage> GetShaderStages() const
    {
        const auto& entryPoints = m_ShaderReflection.GetEntryPointsInfo();

        return {entryPoints.Stages.data(), entryPoints.Count};
    }
    Span<const std::string> GetEntryPoints() const
    {
        const auto& entryPoints = m_ShaderReflection.GetEntryPointsInfo();

        return {entryPoints.Names.data(), entryPoints.Count};
    }
private:
    ShaderReflection m_ShaderReflection{};
    PipelineLayout m_PipelineLayout;
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorsLayouts;
};
