#pragma once

#include "ShaderModule.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"

// todo: probably 4
static constexpr u32 MAX_DESCRIPTOR_SETS = 3;
static_assert(MAX_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");

class ShaderReflection
{
public:
    using ShaderStageInfo = assetLib::ShaderStageInfo;
    using InputAttribute = ShaderStageInfo::InputAttribute;
    using PushConstant = ShaderStageInfo::PushConstant;
    struct SpecializationConstant
    {
        std::string Name;
        u32 Id;
        ShaderStage ShaderStages;
    };
    struct DescriptorSetInfo
    {
        bool HasBindless{false};
        bool HasImmutableSampler{false};
        std::vector<std::string> DescriptorNames{};
        std::vector<DescriptorBinding> Descriptors{};
    };
    using DescriptorSets = std::array<DescriptorSetInfo, MAX_DESCRIPTOR_SETS>;
public:
    static ShaderReflection* ReflectFrom(const std::vector<std::string_view>& paths);
    ShaderReflection() = default;
    ShaderReflection(const ShaderReflection&) = delete;
    ShaderReflection(ShaderReflection&&) = default;
    ShaderReflection& operator=(const ShaderReflection&) = delete;
    ShaderReflection& operator=(ShaderReflection&&) = default;
    ~ShaderReflection();
    
    ShaderStage Stages() const { return m_ShaderStages; }
    const std::vector<SpecializationConstant>& SpecializationConstants() const { return m_SpecializationConstants; }
    const VertexInputDescription& VertexInputDescription() const { return m_VertexInputDescription; }
    const std::vector<PushConstantDescription>& PushConstants() const { return m_PushConstants; }
    const std::array<DescriptorSetInfo, MAX_DESCRIPTOR_SETS>& DescriptorSetsInfo() const
    {
        return m_DescriptorSets;
    }
    DrawFeatures Features() const { return m_Features; }
    const std::vector<ShaderModule>& Shaders() const { return m_Modules; }
private:
    ShaderStageInfo LoadFromAsset(std::string_view path);
private:
    ShaderStage m_ShaderStages{ShaderStage::None};
    std::vector<SpecializationConstant> m_SpecializationConstants;
    ::VertexInputDescription m_VertexInputDescription{};
    std::vector<PushConstantDescription> m_PushConstants{};
    std::array<DescriptorSetInfo, MAX_DESCRIPTOR_SETS> m_DescriptorSets{};
    DrawFeatures m_Features{};
    std::vector<ShaderModule> m_Modules;
};
