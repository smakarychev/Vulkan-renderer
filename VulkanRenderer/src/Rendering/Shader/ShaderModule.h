#pragma once

#include "Containers/Span.h"
#include "Rendering/ResourceHandle.h"

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
};

struct ShaderModuleTag{};
using ShaderModule = ResourceHandleType<ShaderModuleTag>;