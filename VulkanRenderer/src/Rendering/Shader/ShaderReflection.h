#pragma once

#include "ShaderModule.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"

#include <AssetLib/Io/AssetIo.h>

namespace lux
{
namespace assetlib::io
{
class AssetCompressor;
}

namespace assetlib::io
{
class AssetIoInterface;
}

namespace assetlib
{
struct ShaderLoadInfo;
struct ShaderAsset;
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
    static assetlib::io::IoResult<ShaderReflection> Reflect(const assetlib::ShaderAsset& asset,
        const assetlib::ShaderLoadInfo& loadInfo);
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
    const PipelineStateSettings& PipelineSettings() const { return m_PipelineStateSettings; }
    const std::vector<Format>& ColorFormats() const { return m_ColorFormats; }
    const std::optional<Format>& DepthFormat() const { return m_DepthFormat; }
private:
    // todo: remove it and just store the asset?
    ShaderStage m_ShaderStages{ShaderStage::None};
    std::vector<SpecializationConstant> m_SpecializationConstants;
    ::VertexInputDescription m_VertexInputDescription{};
    std::vector<PushConstantDescription> m_PushConstants{};
    DescriptorSets m_DescriptorSets{};
    EntryPointsInfo m_EntryPointsInfo;
    PipelineStateSettings m_PipelineStateSettings{};
    std::vector<Format> m_ColorFormats;
    std::optional<Format> m_DepthFormat;
    std::vector<ShaderModule> m_Modules;
};
}
