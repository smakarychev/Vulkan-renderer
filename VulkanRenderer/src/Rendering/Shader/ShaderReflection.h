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
    struct SpecializationConstant
    {
        std::string Name;
        u32 Id;
        ShaderStage ShaderStages;
    };
    struct DescriptorsInfo
    {
        bool HasBindless{false};
        Sampler ImmutableSampler{};
        std::vector<std::string> DescriptorNames{};
        std::vector<DescriptorBinding> Descriptors{};
    };
    using DescriptorSets = std::array<DescriptorsInfo, MAX_DESCRIPTOR_SETS>;
public:
    static ShaderReflection ReflectFrom(const std::vector<std::string>& paths);
    ShaderReflection() = default;
    ShaderReflection(const ShaderReflection&) = delete;
    ShaderReflection& operator=(const ShaderReflection&) = delete;
    ShaderReflection(ShaderReflection&&) = default;
    ShaderReflection& operator=(ShaderReflection&&) = default;
    ~ShaderReflection();

    ShaderStage Stages() const { return m_ShaderStages; }
    const std::vector<SpecializationConstant>& SpecializationConstants() const { return m_SpecializationConstants; }
    const VertexInputDescription& VertexInputDescription() const { return m_VertexInputDescription; }
    const std::vector<PushConstantDescription>& PushConstants() const { return m_PushConstants; }
    const std::array<DescriptorsInfo, MAX_DESCRIPTOR_SETS>& DescriptorSetsInfo() const
    {
        return m_DescriptorSets;
    }
    const std::vector<ShaderModule>& Shaders() const { return m_Modules; }
private:
    ShaderStage m_ShaderStages{ShaderStage::None};
    std::vector<SpecializationConstant> m_SpecializationConstants;
    ::VertexInputDescription m_VertexInputDescription{};
    std::vector<PushConstantDescription> m_PushConstants{};
    std::array<DescriptorsInfo, MAX_DESCRIPTOR_SETS> m_DescriptorSets{};
    std::vector<ShaderModule> m_Modules;
};
