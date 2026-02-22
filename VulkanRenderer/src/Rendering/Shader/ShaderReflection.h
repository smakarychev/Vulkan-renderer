#pragma once

#include "ShaderModule.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "v2/Io/AssetIo.h"

namespace lux::assetlib::io
{
class AssetCompressor;
}

namespace lux::assetlib::io
{
class AssetIoInterface;
}

namespace lux::assetlib
{
struct ShaderHeader;
}

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
    struct EntryPointsInfo
    {
        std::array<ShaderStage, MAX_PIPELINE_SHADER_COUNT> Stages;
        std::array<std::string, MAX_PIPELINE_SHADER_COUNT> Names;
        u32 Count{0};
    };
public:
    static lux::assetlib::io::IoResult<ShaderReflection> Reflect(const std::filesystem::path& path,
        lux::assetlib::io::AssetIoInterface& io, lux::assetlib::io::AssetCompressor& compressor);
    ShaderReflection() = default;
    ShaderReflection(const ShaderReflection&) = delete;
    ShaderReflection& operator=(const ShaderReflection&) = delete;
    ShaderReflection(ShaderReflection&& other) noexcept;
    ShaderReflection& operator=(ShaderReflection&& other) noexcept;
    ~ShaderReflection();

    ShaderStage Stages() const { return m_ShaderStages; }
    const std::vector<SpecializationConstant>& SpecializationConstants() const { return m_SpecializationConstants; }
    const VertexInputDescription& VertexInputDescription() const { return m_VertexInputDescription; }
    const std::vector<PushConstantDescription>& PushConstants() const { return m_PushConstants; }
    const DescriptorSets& DescriptorSetsInfo() const
    {
        return m_DescriptorSets;
    }
    const EntryPointsInfo& GetEntryPointsInfo() const { return m_EntryPointsInfo; }
    const std::vector<ShaderModule>& Shaders() const { return m_Modules; }
private:
    // todo: remove it and just store the asset?
    ShaderStage m_ShaderStages{ShaderStage::None};
    std::vector<SpecializationConstant> m_SpecializationConstants;
    ::VertexInputDescription m_VertexInputDescription{};
    std::vector<PushConstantDescription> m_PushConstants{};
    DescriptorSets m_DescriptorSets{};
    EntryPointsInfo m_EntryPointsInfo;
    std::vector<ShaderModule> m_Modules;
};
