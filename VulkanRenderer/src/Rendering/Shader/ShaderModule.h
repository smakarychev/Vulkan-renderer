#pragma once

#include "Common/Span.h"
#include "Rendering/DescriptorsTraits.h"
#include "Rendering/ResourceHandle.h"

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
    ShaderStage Stage{ShaderStage::None};
};

class ShaderModule
{
    FRIEND_INTERNAL
public:
    ResourceHandleType<ShaderModule> Handle() const { return m_ResourceHandle; }
private:
    // todo: change once handles are ready
    ResourceHandleType<ShaderModule> m_ResourceHandle{};
};
