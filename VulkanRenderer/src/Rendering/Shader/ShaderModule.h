#pragma once

#include "Common/Span.h"
#include "Rendering/DescriptorsTraits.h"
#include "Rendering/ResourceHandle.h"

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
    ShaderStage Stage{ShaderStage::None};
};

struct ShaderModuleTag {};
using ShaderModule = ResourceHandleType<ShaderModuleTag>;