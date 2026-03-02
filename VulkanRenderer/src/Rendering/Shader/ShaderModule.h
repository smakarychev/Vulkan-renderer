#pragma once

#include "Rendering/ResourceHandle.h"

#include <CoreLib/Containers/Span.h>

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
};

struct ShaderModuleTag{};
using ShaderModule = ResourceHandleType<ShaderModuleTag>;