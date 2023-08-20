#pragma once

#include "types.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
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

struct BufferData
{
    VkBuffer Buffer{VK_NULL_HANDLE};
    VkDeviceMemory BufferMemory{VK_NULL_HANDLE};
};

struct TextureData
{
    VkImage Texture{VK_NULL_HANDLE};
    VkImageView View{VK_NULL_HANDLE};
    VkDeviceMemory TextureMemory{VK_NULL_HANDLE};
};

struct Vertex
{
    glm::vec3 Position{};
    glm::vec3 Color{};
    glm::vec2 UV{};
    bool operator==(const Vertex& other) const
    {
        return Position == other.Position && Color == other.Color && UV == other.UV;
    }
    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 3> GetAttributesDescription()
    {
        VkVertexInputAttributeDescription positionDescription = {};
        positionDescription.binding = 0;
        positionDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionDescription.location = 0;
        positionDescription.offset = offsetof(Vertex, Position);

        VkVertexInputAttributeDescription colorDescription = {};
        colorDescription.binding = 0;
        colorDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
        colorDescription.location = 1;
        colorDescription.offset = offsetof(Vertex, Color);

        VkVertexInputAttributeDescription uvDescription = {};
        uvDescription.binding = 0;
        uvDescription.format = VK_FORMAT_R32G32_SFLOAT;
        uvDescription.location = 2;
        uvDescription.offset = offsetof(Vertex, UV);

        return { positionDescription, colorDescription, uvDescription };
    }
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(const Vertex& vertex) const noexcept;
    };
}

struct TransformUBO
{
    glm::mat4 Model{};
    glm::mat4 View{};
    glm::mat4 Projection{};
};

struct BufferCreateData
{
    VkDeviceSize SizeBytes;
    VkBufferUsageFlags Usage;
    VkMemoryPropertyFlags Properties;
};

struct TextureCreateData
{
    u32 Width;
    u32 Height;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags Usage;
    VkMemoryPropertyFlags Properties;
    u32 MipmapLevels{1};
    VkSampleCountFlagBits Samples{VK_SAMPLE_COUNT_1_BIT};
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
    void OnUpdate();
    void CleanUp();

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateSwapchainImageViews();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateColorResources();
    void CreateDepthResources();
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();
    void LoadModel();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
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

    BufferData CreateBuffer(const BufferCreateData& bufferCreateData);
    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize sizeBytes);
    u32 FindMemoryType(u32 filter, VkMemoryPropertyFlags properties);

    TextureData CreateTexture(const TextureCreateData& textureCreateData);
    void TransitionTextureLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, u32 mipmapLevels);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
    void GenerateMipmaps(VkImage image, VkFormat format, u32 width, u32 height, u32 mipmapLevels);

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipmapLevels);
    
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer cmd);

    VkFormat GetDepthFormat();
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool HasStencilComponent(VkFormat format);

    VkSampleCountFlagBits GetMaxSamplesCount();
    
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
    BufferData m_VertexBuffer;
    std::vector<u32> m_Indices;
    BufferData m_IndexBuffer;

    VkDescriptorSetLayout m_DescriptorSetLayout{VK_NULL_HANDLE};
    std::vector<BufferData> m_UniformBuffers;
    std::vector<void*> m_UniformBuffersMapped;

    VkDescriptorPool m_DescriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_DescriptorSets;

    TextureData m_TextureImage;
    VkSampler m_TextureImageSampler{VK_NULL_HANDLE};
    u32 m_TextureImageMipmapLevels{1};

    TextureData m_DepthTexture;

    VkSampleCountFlagBits m_MSAASamples{VK_SAMPLE_COUNT_1_BIT};

    TextureData m_ColorTexture;

    static constexpr std::string_view TEXTURE_PATH = "assets/models/vokselia_spawn/vokselia_spawn.png";
    static constexpr std::string_view MODEL_PATH = "assets/models/vokselia_spawn/vokselia_spawn.obj";
};
