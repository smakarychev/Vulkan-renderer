#pragma once

#include "Vulkan/DeviceResourceListHandle.h"

#include <CoreLib/Containers/Span.h>

struct ShaderModuleCreateInfo
{
    Span<const std::byte> Source{};
};

struct ShaderModuleTag{};
using ShaderModule = lux::DeviceResourceListHandle<ShaderModuleTag>;