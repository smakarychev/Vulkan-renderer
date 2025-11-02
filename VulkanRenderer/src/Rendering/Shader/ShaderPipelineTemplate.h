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
    ShaderReflection* ShaderReflection{nullptr};
    Span<const ShaderStage> ShaderStages{};
    Span<const std::string> ShaderEntryPoints{};
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

    const ShaderReflection& GetReflection() const { return *m_ShaderReflection; }

    Span<const ShaderStage> GetShaderStages() const { return Span(m_ShaderStages.data(), m_ShaderStageCount); }
    Span<const std::string> GetEntryPoints() const { return Span(m_ShaderEntryPoints.data(), m_ShaderStageCount); }
private:
    ShaderReflection* m_ShaderReflection{nullptr};
    PipelineLayout m_PipelineLayout;
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorsLayouts;
    std::array<ShaderStage, MAX_PIPELINE_SHADER_COUNT> m_ShaderStages;
    std::array<std::string, MAX_PIPELINE_SHADER_COUNT> m_ShaderEntryPoints;
    u32 m_ShaderStageCount{0};
};
