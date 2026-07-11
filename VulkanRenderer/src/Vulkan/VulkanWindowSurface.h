#pragma once

#include "Core/Window/Window.h"
#include <CoreLib/Containers/Span.h>

namespace lux
{
class VulkanWindowSurface : public WindowSurface
{
public:
    bool Init(void* instance, void** surface) const;
    static Span<const char*> GetRequiredExtensions();
};
}
