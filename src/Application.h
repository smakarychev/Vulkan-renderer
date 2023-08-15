#pragma once

#include "types.h"
#include "glm/glm.hpp"

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

struct FrameData
{
    VkCommandBuffer m_CommandBuffer{VK_NULL_HANDLE};
    VkSemaphore m_ImageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore m_ImageRenderedSemaphore{VK_NULL_HANDLE};
    VkFence m_ImageAvailableFence{VK_NULL_HANDLE};
};

struct Vertex
{
    glm::vec2 Position{};
    glm::vec3 Color{};
    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 2> GetAttributesDescription()
    {
        VkVertexInputAttributeDescription positionDescription = {};
        positionDescription.binding = 0;
        positionDescription.format = VK_FORMAT_R32G32_SFLOAT;
        positionDescription.location = 0;
        positionDescription.offset = offsetof(Vertex, Position);

        VkVertexInputAttributeDescription colorDescription = {};
        colorDescription.binding = 0;
        colorDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorDescription.location = 1;
        colorDescription.offset = offsetof(Vertex, Color);

        return { positionDescription, colorDescription };
    }
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
    void OnDraw();
    void CleanUp();

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateSwapchainImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateVertexBuffer();
    void CreateCommandBuffer();
    void RecordCommandBuffer(VkCommandBuffer cmd, u32 imageIndex);
    void CreateSynchronizationPrimitives();

    void RecreateSwapchain();
    void CleanUpSwapchain();

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

    VkShaderModule CreateShaderModule(const std::vector<u32>& spirv);

    u32 FindMemoryType(u32 filter, VkMemoryPropertyFlags properties);
    
private:
    GLFWwindow* m_Window{nullptr};
    WindowProps m_WindowProps{};
    bool m_WindowResized{false};


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

    VkRenderPass m_RenderPass{VK_NULL_HANDLE};
    VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_Pipeline{VK_NULL_HANDLE};

    std::vector<VkFramebuffer> m_Framebuffers;
    
    std::vector<VkImageView> m_SwapchainImageViews;

    VkCommandPool m_CommandPool{VK_NULL_HANDLE};

    std::vector<FrameData> m_BufferedFrames;
    u32 m_CurrentFrameToRender{0};
    static constexpr u32 BUFFERED_FRAMES_COUNT{2};

    std::vector<Vertex> m_Vertices;
    VkBuffer m_VertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_VertexBufferMemory{VK_NULL_HANDLE};
};
