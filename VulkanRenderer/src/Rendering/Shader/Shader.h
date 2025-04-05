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
    DescriptorAllocator Allocator{};
    DescriptorArenaAllocator ResourceAllocator{};
    DescriptorArenaAllocator SamplerAllocator{};
};

class ShaderPipelineTemplate
{
    friend class ShaderDescriptorSet;
public:
    ShaderPipelineTemplate() = default;
    ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo&& createInfo);
    
    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    DescriptorBindingInfo GetBinding(u32 set, std::string_view name) const;
    std::optional<DescriptorBindingInfo> TryGetBinding(u32 set, std::string_view name) const;
    std::pair<u32, DescriptorBindingInfo> GetSetAndBinding(std::string_view name) const;
    std::array<bool, MAX_DESCRIPTOR_SETS> GetSetPresence() const;

    DescriptorArenaAllocator GetAllocator(DescriptorsKind kind) const;

    bool IsComputeTemplate() const;

    VertexInputDescription CreateCompatibleVertexDescription(const VertexInputDescription& compatibleTo) const;

    const ShaderReflection& GetReflection() const { return *m_ShaderReflection; }
private:
    struct Allocator
    {
        DescriptorAllocator DescriptorAllocator{};
        DescriptorArenaAllocator ResourceAllocator{};    
        DescriptorArenaAllocator SamplerAllocator{};       
    };
    Allocator m_Allocator{};
    bool m_UseDescriptorBuffer{false};

    PipelineLayout m_PipelineLayout;
    ShaderReflection* m_ShaderReflection{nullptr};
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorsLayouts;
};

struct ShaderDescriptorSetCreateInfo
{
    ShaderPipelineTemplate* ShaderPipelineTemplate;
    std::array<std::optional<DescriptorSetCreateInfo>, MAX_DESCRIPTOR_SETS> DescriptorInfos;
};

class ShaderTemplateLibrary
{
public:
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string>& paths,
        StringId name, DescriptorAllocator allocator);
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string>& paths,
        StringId name, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(StringId name, DescriptorArenaAllocators& allocators);
    
    static ShaderPipelineTemplate* CreateMaterialsTemplate(StringId name, DescriptorArenaAllocators& allocators);

    static ShaderPipelineTemplate* ReloadShaderPipelineTemplate(const std::vector<std::string>& paths,
        StringId name, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(StringId name);
    static StringId GenerateTemplateName(StringId name, DescriptorAllocator allocator);
    static StringId GenerateTemplateName(StringId name, DescriptorArenaAllocators& allocators);
    static void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, StringId name);
private:
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string>& paths,
        DescriptorAllocator allocator);
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string>& paths,
        DescriptorArenaAllocators& allocators);
private:
    static std::unordered_map<StringId, ShaderPipelineTemplate> s_Templates;
};

