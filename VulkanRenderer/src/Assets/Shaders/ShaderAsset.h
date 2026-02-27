#pragma once
#include "Assets/AssetHandle.h"
#include "Rendering/Pipeline.h"

namespace lux
{
class ShaderAsset
{
    friend class ShaderAssetManager;
public:
    Pipeline Pipeline() const { return m_Pipeline; }
    PipelineLayout GetLayout() const { return m_PipelineLayout; }
    const ::Descriptors& Descriptors(u32 index) const { return m_Descriptors[index]; }
    const ::DescriptorsLayout& DescriptorsLayout(u32 index) const { return m_DescriptorLayouts[index]; }
private:
    ::Pipeline m_Pipeline{};
    PipelineLayout m_PipelineLayout{};
    std::array<::Descriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    std::array<::DescriptorsLayout, MAX_DESCRIPTOR_SETS> m_DescriptorLayouts;
};

using ShaderHandle = AssetHandle<ShaderAsset>;
}
