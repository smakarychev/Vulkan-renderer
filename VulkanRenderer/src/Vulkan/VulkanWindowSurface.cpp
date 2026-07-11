#include "rendererpch.h"
#include "VulkanWindowSurface.h"

#include <CoreLib/types.h>

#include <volk.h>
#include <GLFW/glfw3.h>

namespace lux
{
bool VulkanWindowSurface::Init(void* instance, void** surface) const
{
    return glfwCreateWindowSurface((VkInstance)instance, (GLFWwindow*)m_NativeWindow, nullptr,
        (VkSurfaceKHR*)surface) == VK_SUCCESS;
}

Span<const char*> VulkanWindowSurface::GetRequiredExtensions()
{
    u32 count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    return Span{extensions, count};
}
}
