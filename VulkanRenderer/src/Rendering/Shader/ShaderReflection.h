#pragma once

#include "ShaderModule.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "v2/AssetLibV2.h"

namespace assetlib
{
struct ShaderHeader;
}

struct ShaderReflectionEntryPointsInfo
{
    std::array<ShaderStage, MAX_PIPELINE_SHADER_COUNT> Stages;
    std::array<std::string, MAX_PIPELINE_SHADER_COUNT> Names;
    u32 Count{0};
};

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
        std::vector<DescriptorBinding> Descriptors{};
    };
    using DescriptorSets = std::array<DescriptorsInfo, MAX_DESCRIPTOR_SETS>;
public:
    static ShaderReflection ReflectFrom(const std::vector<std::string>& paths);
    // todo: temp name, rename once only slang is left
    static assetlib::io::IoResult<ShaderReflection> ReflectFromSlang(const std::filesystem::path& path);
    ShaderReflection() = default;
    ShaderReflection(const ShaderReflection&) = delete;
    ShaderReflection& operator=(const ShaderReflection&) = delete;
    ShaderReflection(ShaderReflection&& other) noexcept;
    ShaderReflection& operator=(ShaderReflection&& other) noexcept;
    ~ShaderReflection();

    static ShaderReflectionEntryPointsInfo GetEntryPointsInfo(const assetlib::ShaderHeader& shader);

    // todo: delete once ready
    static ShaderStage GetShaderStage(u32 shaderStage);
    
    ShaderStage Stages() const { return m_ShaderStages; }
    const std::vector<SpecializationConstant>& SpecializationConstants() const { return m_SpecializationConstants; }
    const VertexInputDescription& VertexInputDescription() const { return m_VertexInputDescription; }
    const std::vector<PushConstantDescription>& PushConstants() const { return m_PushConstants; }
    const DescriptorSets& DescriptorSetsInfo() const
    {
        return m_DescriptorSets;
    }
    const std::vector<ShaderModule>& Shaders() const { return m_Modules; }
private:
    // todo: remove it and just store the asset?
    ShaderStage m_ShaderStages{ShaderStage::None};
    std::vector<SpecializationConstant> m_SpecializationConstants;
    ::VertexInputDescription m_VertexInputDescription{};
    std::vector<PushConstantDescription> m_PushConstants{};
    DescriptorSets m_DescriptorSets{};
    std::vector<ShaderModule> m_Modules;
};
