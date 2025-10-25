#pragma once

#include <array>

#include "ShaderReflection.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "types.h"

class RenderCommandList;
struct PushConstantDescription;
class DescriptorLayoutCache;

struct ShaderPipelineTemplateCreateInfo
{
    ShaderReflection* ShaderReflection{nullptr};
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> DescriptorLayoutOverrides{};
};

class ShaderPipelineTemplate
{
public:
    ShaderPipelineTemplate() = default;
    ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo&& createInfo);
    
    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    DescriptorSlotInfo GetBinding(u32 set, std::string_view name) const;
    std::optional<DescriptorSlotInfo> TryGetBinding(u32 set, std::string_view name) const;
    std::pair<u32, DescriptorSlotInfo> GetSetAndBinding(std::string_view name) const;
    std::array<bool, MAX_DESCRIPTOR_SETS> GetSetPresence() const;

    bool IsComputeTemplate() const;

    VertexInputDescription CreateCompatibleVertexDescription(const VertexInputDescription& compatibleTo) const;

    const ShaderReflection& GetReflection() const { return *m_ShaderReflection; }
private:
    ShaderReflection* m_ShaderReflection{nullptr};
    PipelineLayout m_PipelineLayout;
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorsLayouts;
};
