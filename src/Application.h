#pragma once

#include "types.h"

#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

struct WindowProps
{
    i32 Width{1600};
    i32 Height{900};
    std::string_view Name{"VulkanApp"};
};

class Application
{
public:
    void Run();
private:
    void Init();
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void CleanUp();

    std::vector<const char*> GetRequiredExtensions();
    bool CheckExtensions(const std::vector<const char*>& requiredExtensions);

    std::vector<const char*> GetRequiredValidationLayers();
    bool CheckValidationLayers(const std::vector<const char*>& requiredLayers);
private:
    GLFWwindow* m_Window{nullptr};
    WindowProps m_WindowProps{};

    VkInstance m_Instance;
};
