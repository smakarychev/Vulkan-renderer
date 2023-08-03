#pragma once

#include "types.h"
#include <vulkan/vulkan_core.h>

#include <array>
#include <vector>
#include <optional>
#include <string_view>


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
    std::array<u32, 2> AsArray() const { return { *GraphicsFamily, *PresentationFamily }; }
};

struct SwapchainDetails
{
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
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
    void CreateSwapchain();
    void CreateSwapchainImageViews();

    std::vector<const char*> GetRequiredInstanceExtensions();
    bool CheckInstanceExtensions(const std::vector<const char*>& requiredExtensions);

    std::vector<const char*> GetRequiredValidationLayers();
    bool CheckValidationLayers(const std::vector<const char*>& requiredLayers);

    std::vector<const char*> GetRequiredDeviceExtensions();
    bool CheckDeviceExtensions(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions);

    bool IsDeviceSuitable(VkPhysicalDevice device);

    QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
    SwapchainDetails GetSwapchainDetails(VkPhysicalDevice device);

    VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    u32 ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& capabilities);
    
private:
    GLFWwindow* m_Window{nullptr};
    WindowProps m_WindowProps{};

    VkInstance m_Instance{VK_NULL_HANDLE};
    VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentationQueue{VK_NULL_HANDLE};
    VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};
    std::vector<VkImage> m_SwapchainImages;
    VkSurfaceFormatKHR m_SwapchainFormat{};
    VkExtent2D m_SwapchainExtent{};

    std::vector<VkImageView> m_SwapchainImageViews;
};
