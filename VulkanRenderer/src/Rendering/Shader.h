#pragma once

#include <array>
#include <vector>

#include "Descriptors.h"
#include "DescriptorsTraits.h"
#include "Pipeline.h"
#include "ShaderAsset.h"
#include "types.h"

class DescriptorSet;
struct PushConstantDescription;
class DescriptorsLayout;
class DescriptorLayoutCache;
class DescriptorAllocator;
class Image;

// todo: this is probably deprecated
static constexpr u32 MAX_PIPELINE_DESCRIPTOR_SETS = 3;
static_assert(MAX_PIPELINE_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");
enum class DescriptorKind : u32
{
    Global = 0,
    Pass = 1,
    Material = 2
};


// todo: these probably should not be here, but having them as constexpr is quite useful
// since we can use them in constexpr hash map, which is not used currently, but might be used in the future,
// otherwise having them as CVars is preferable
// also it may be possible, that these should be somewhere in the AssetLib, to generate shader code for us,
static constexpr std::string_view UNIFORM_POSITIONS         = "u_positions";
static constexpr std::string_view UNIFORM_NORMALS           = "u_normals";
static constexpr std::string_view UNIFORM_TANGENTS          = "u_tangents";
static constexpr std::string_view UNIFORM_UV                = "u_uv";
static constexpr std::string_view UNIFORM_MATERIALS         = "u_materials";
static constexpr std::string_view UNIFORM_TEXTURES          = "u_textures";
static constexpr std::string_view UNIFORM_SSAO_TEXTURE      = "u_ssao_texture";
static constexpr std::string_view UNIFORM_IRRADIANCE_SH     = "u_irradiance_SH";
static constexpr std::string_view UNIFORM_PREFILTER_MAP     = "u_prefilter_map";
static constexpr std::string_view UNIFORM_BRDF              = "u_brdf";
static constexpr std::string_view UNIFORM_TRIANGLES         = "u_triangles";

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
    ShaderStage Stage{ShaderStage::None};
};

class ShaderReflection
{
public:
    struct ReflectionData
    {
        struct SpecializationConstant
        {
            std::string Name;
            u32 Id;
            ShaderStage ShaderStages;
        };
        using InputAttribute = assetLib::ShaderStageInfo::InputAttribute;
        using PushConstant = assetLib::ShaderStageInfo::PushConstant;
        struct DescriptorSet
        {
            struct DescriptorSetBindingNamedFlagged
            {
                std::string Name;
                DescriptorBinding Descriptor;
            };
            u32 Set; 
            bool HasBindless{false};
            bool HasImmutableSampler{false};
            std::vector<DescriptorSetBindingNamedFlagged> Descriptors;
        };

        ShaderStage ShaderStages;
        std::vector<SpecializationConstant> SpecializationConstants;
        std::vector<InputAttribute> InputAttributes;
        std::vector<PushConstant> PushConstants;
        std::vector<DescriptorSet> DescriptorSets;

        DrawFeatures Features{};
    };
public:
    static ShaderReflection* ReflectFrom(const std::vector<std::string_view>& paths);
    ShaderReflection() = default;
    ShaderReflection(const ShaderReflection&) = delete;
    ShaderReflection(ShaderReflection&&) = default;
    ShaderReflection& operator=(const ShaderReflection&) = delete;
    ShaderReflection& operator=(ShaderReflection&&) = default;
    ~ShaderReflection();
    
    const ReflectionData& GetReflectionData() const { return m_ReflectionData; }
    const std::vector<ShaderModule>& GetShaders() const { return m_Modules; }
private:
    assetLib::ShaderStageInfo LoadFromAsset(std::string_view path);
    static assetLib::ShaderStageInfo MergeReflections(const assetLib::ShaderStageInfo& first,
        const assetLib::ShaderStageInfo& second);
    static std::vector<ReflectionData::DescriptorSet> ProcessDescriptorSets(
        const std::vector<assetLib::ShaderStageInfo::DescriptorSet>& sets);
    static DrawFeatures ExtractDrawFeatures(const std::vector<assetLib::ShaderStageInfo::DescriptorSet>& descriptorSets,
        const std::vector<ReflectionData::InputAttribute>& inputs);
private:
    std::vector<ShaderModule> m_Modules;
    ReflectionData m_ReflectionData{};
};

class ShaderPipelineTemplate
{
    friend class ShaderPipeline;
    friend class ShaderDescriptorSet;
    friend class ShaderDescriptors;
private:
    struct DescriptorsFlags
    {
        std::vector<DescriptorBinding> Descriptors;
        std::vector<DescriptorFlags> Flags;
    };
public:
    using ReflectionData = ShaderReflection::ReflectionData;
    class Builder
    {
        friend class ShaderPipelineTemplate;
        struct CreateInfo
        {
            ShaderReflection* ShaderReflection;
            DescriptorAllocator* Allocator{nullptr};
            DescriptorArenaAllocator* ResourceAllocator{nullptr};
            DescriptorArenaAllocator* SamplerAllocator{nullptr};
        };
    public:
        ShaderPipelineTemplate Build();
        Builder& SetShaderReflection(ShaderReflection* shaderReflection);
        Builder& SetDescriptorAllocator(DescriptorAllocator* allocator);
        Builder& SetDescriptorArenaResourceAllocator(DescriptorArenaAllocator* allocator);
        Builder& SetDescriptorArenaSamplerAllocator(DescriptorArenaAllocator* allocator);
    private:
        CreateInfo m_CreateInfo;
    };

    struct DescriptorSetInfo
    {
        std::vector<std::string> Names;
        std::vector<DescriptorBinding> Bindings;
    };

    using SpecializationConstant = ShaderReflection::ReflectionData::SpecializationConstant;
    
public:
    static ShaderPipelineTemplate Create(const Builder::CreateInfo& createInfo);

    PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    const DescriptorsLayout GetDescriptorsLayout(u32 index) const { return m_DescriptorsLayouts[index]; }

    const DescriptorBinding& GetBinding(u32 set, std::string_view name) const;
    const DescriptorBinding* TryGetBinding(u32 set, std::string_view name) const;
    std::pair<u32, const DescriptorBinding&> GetSetAndBinding(std::string_view name) const;
    std::array<bool, MAX_PIPELINE_DESCRIPTOR_SETS> GetSetPresence() const;
    
    bool IsComputeTemplate() const { return m_IsComputeTemplate; }

    DrawFeatures GetDrawFeatures() const { return m_Features; }

    VertexInputDescription CreateCompatibleVertexDescription(const VertexInputDescription& compatibleTo) const;

    const std::vector<ShaderModule>& GetShaders() const { return m_ShaderReflection->GetShaders(); }
    const std::vector<SpecializationConstant>& GetSpecializations() const
    {
        return m_ShaderReflection->GetReflectionData().SpecializationConstants;
    }
    
private:
    static std::vector<DescriptorsLayout> CreateDescriptorLayouts(
        const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections, bool useDescriptorBuffer);
    static VertexInputDescription CreateInputDescription(
        const std::vector<ReflectionData::InputAttribute>& inputAttributeReflections);
    static std::vector<PushConstantDescription> CreatePushConstantDescriptions(
        const std::vector<ReflectionData::PushConstant>& pushConstantReflections);
    static DescriptorsFlags ExtractDescriptorsAndFlags(const ReflectionData::DescriptorSet& descriptorSet,
        bool useDescriptorBuffer);
private:
    struct Allocator
    {
        DescriptorAllocator* DescriptorAllocator{nullptr};
        DescriptorArenaAllocator* ResourceAllocator{nullptr};    
        DescriptorArenaAllocator* SamplerAllocator{nullptr};       
    };
    Allocator m_Allocator{};
    bool m_UseDescriptorBuffer{false};

    // todo: change to handles once ready
    PipelineLayout m_PipelineLayout;
    ShaderReflection* m_ShaderReflection{nullptr};
    VertexInputDescription m_VertexInputDescription;
    std::vector<DescriptorsLayout> m_DescriptorsLayouts;

    std::vector<SpecializationConstant> m_SpecializationConstants;
    std::array<DescriptorSetInfo, MAX_PIPELINE_DESCRIPTOR_SETS> m_DescriptorSetsInfo;
    std::vector<DescriptorPoolFlags> m_DescriptorPoolFlags;
    u32 m_DescriptorSetCount;

    bool m_IsComputeTemplate{false};

    DrawFeatures m_Features{};
};

struct ShaderDescriptorSetCreateInfo
{
    ShaderPipelineTemplate* ShaderPipelineTemplate;
    std::array<std::optional<DescriptorSetCreateInfo>, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorInfos;
};

class ShaderDescriptorSet
{
    using Texture = Image;
public:
    struct DescriptorSetsInfo
    {
        struct SetInfo
        {
            bool IsPresent{false};
            DescriptorSet Set; 
        };
        std::array<SetInfo, MAX_PIPELINE_DESCRIPTOR_SETS> DescriptorSets;
        u32 DescriptorCount{0};
    };
public:
    ShaderDescriptorSet(ShaderDescriptorSetCreateInfo&& createInfo);
    
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout)
        const;
    void BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout,
        const std::vector<u32>& dynamicOffsets) const;

    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout)
        const;
    void BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind, PipelineLayout pipelineLayout,
                     const std::vector<u32>& dynamicOffsets) const;

    void SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex);
    
    const DescriptorSetsInfo& GetDescriptorSetsInfo() const { return m_DescriptorSetsInfo; }
    const DescriptorSet& GetDescriptorSet(DescriptorKind kind) const
    {
        return m_DescriptorSetsInfo.DescriptorSets[(u32)kind].Set;
    }
private:
    ShaderPipelineTemplate* m_Template{nullptr};
    
    DescriptorSetsInfo m_DescriptorSetsInfo{};
};

enum class ShaderDescriptorsKind
{
    Sampler = 0, Resource = 1, Materials = 2,
    MaxVal
};

class ShaderDescriptors
{
    friend class ShaderCache;
public:
    using Texture = Image;
    class Builder
    {
        friend class ShaderDescriptors;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            const ShaderPipelineTemplate* ShaderPipelineTemplate{nullptr};
            DescriptorArenaAllocator* Allocator{nullptr};
            u32 Set{0};
            u32 BindlessCount{0};
        };
    public:
        ShaderDescriptors Build();
        Builder& SetTemplate(const ShaderPipelineTemplate* shaderPipelineTemplate,
            DescriptorAllocatorKind allocatorKind);
        Builder& ExtractSet(u32 set);
        Builder& BindlessCount(u32 count);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    using BindingInfo = Descriptors::BindingInfo;
    
    static ShaderDescriptors Create(const Builder::CreateInfo& createInfo);

    void BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout) const;
    void BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout) const;
    void BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const;
    void BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const;

    void UpdateBinding(std::string_view name, const BufferBindingInfo& buffer) const;
    void UpdateBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateBinding(std::string_view name, const TextureBindingInfo& texture) const;
    void UpdateBinding(std::string_view name, const TextureBindingInfo& texture, u32 index) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture, u32 index) const;

    void UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer) const;
    void UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture) const;
    void UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture, u32 index) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
        u32 index) const;

    BindingInfo GetBindingInfo(std::string_view bindingName) const;
    
private:
    Descriptors m_Descriptors{};
    u32 m_SetNumber{0};
    
    const ShaderPipelineTemplate* m_Template{nullptr};
};

class ShaderTemplateLibrary
{
public:
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorAllocator& allocator);
    static ShaderPipelineTemplate* LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(const std::string& name, DescriptorArenaAllocators& allocators);
    
    static ShaderPipelineTemplate* CreateMaterialsTemplate(const std::string& templateName,
        DescriptorArenaAllocators& allocators);

    static ShaderPipelineTemplate* ReloadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
        std::string_view templateName, DescriptorArenaAllocators& allocators);
    static ShaderPipelineTemplate* GetShaderTemplate(const std::string& name);
    static std::string GenerateTemplateName(std::string_view templateName, DescriptorAllocator& allocator);
    static std::string GenerateTemplateName(std::string_view templateName, DescriptorArenaAllocators& allocators);
    static void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name);
private:
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string_view>& paths,
        DescriptorAllocator& allocator);
    static ShaderPipelineTemplate CreateFromPaths(const std::vector<std::string_view>& paths,
        DescriptorArenaAllocators& allocators);
private:
    static std::unordered_map<std::string, ShaderPipelineTemplate> s_Templates;
};