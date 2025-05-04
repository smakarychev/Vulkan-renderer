#pragma once

#include <array>
#include <vector>

#include "ShaderReflection.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "types.h"
#include "String/StringId.h"


class RenderCommandList;
struct PushConstantDescription;
class DescriptorLayoutCache;

enum class DescriptorKind : u32
{
    Global = 0,
    Pass = 1,
    Material = 2
};

static constexpr u32 BINDLESS_DESCRIPTORS_INDEX = 2;
static_assert(BINDLESS_DESCRIPTORS_INDEX == 2, "Bindless descriptors are expected to be at index 2");

struct ShaderPipelineTemplateCreateInfo
{
    ShaderReflection* ShaderReflection{nullptr};
};

class ShaderPipelineTemplate
{
public:
    ShaderPipelineTemplate() = default;
    ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo&& createInfo);
    
    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    DescriptorBindingInfo GetBinding(u32 set, std::string_view name) const;
    std::optional<DescriptorBindingInfo> TryGetBinding(u32 set, std::string_view name) const;
    std::pair<u32, DescriptorBindingInfo> GetSetAndBinding(std::string_view name) const;
    std::array<bool, MAX_DESCRIPTOR_SETS> GetSetPresence() const;

    bool IsComputeTemplate() const;

    VertexInputDescription CreateCompatibleVertexDescription(const VertexInputDescription& compatibleTo) const;

    const ShaderReflection& GetReflection() const { return *m_ShaderReflection; }
private:
    ShaderReflection* m_ShaderReflection{nullptr};
    PipelineLayout m_PipelineLayout;
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorsLayouts;
};

class ShaderTemplateLibrary
{
public:
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string>& paths,
        StringId name);
    static ShaderPipelineTemplate* GetShaderTemplate(StringId name);
    static ShaderPipelineTemplate* ReloadShaderPipelineTemplate(const std::vector<std::string>& paths,
        StringId name);
    static void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, StringId name);
private:
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string>& paths);
private:
    static std::unordered_map<StringId, ShaderPipelineTemplate> s_Templates;
};

