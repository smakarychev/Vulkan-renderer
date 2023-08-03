#pragma once

#include <optional>

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

struct QueueFamilyIndices
{
    std::optional<u32> GraphicsFamily;
    std::optional<u32> PresentationFamily;
    bool IsComplete() const { return GraphicsFamily.has_value() && PresentationFamily.has_value(); }
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

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    std::vector<const char*> GetRequiredExtensions();
    bool CheckExtensions(const std::vector<const char*>& requiredExtensions);

    std::vector<const char*> GetRequiredValidationLayers();
    bool CheckValidationLayers(const std::vector<const char*>& requiredLayers);

    bool IsDeviceSuitable(VkPhysicalDevice device);

    QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
private:
    GLFWwindow* m_Window{nullptr};
    WindowProps m_WindowProps{};

    VkInstance m_Instance{VK_NULL_HANDLE};
    VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentationQueue{VK_NULL_HANDLE};
};
