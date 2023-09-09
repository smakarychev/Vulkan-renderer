#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "Mesh.h"
#include "Scene.h"
#include "Vulkan/VulkanInclude.h"

#include <array>

// todo: should not be here obv
struct CameraData
{
    glm::mat4 View;
    glm::mat4 Projection;
    glm::mat4 ViewProjection;
};

struct CameraDataUBO
{
    Buffer Buffer;
    CameraData CameraData;
};

struct SceneData
{
    glm::vec4 FogColor;             // w is for exponent
    glm::vec4 FogDistances;         //x for min, y for max, zw unused.
    glm::vec4 AmbientColor;
    glm::vec4 SunlightDirection;    //w for sun power
    glm::vec4 SunlightColor;
};

struct SceneDataUBO
{
    Buffer Buffer;
    SceneData SceneData;
};

static constexpr u32 MAX_OBJECTS = 10'000;

struct ObjectData
{
    glm::mat4 Transform;
};

struct ObjectDataSSBO
{
    Buffer Buffer;
    std::array<ObjectData, MAX_OBJECTS> Objects;
};

struct FrameContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    SwapchainFrameSync FrameSync;
    u32 FrameNumber;
    DescriptorSet GlobalDescriptorSet;
    CameraDataUBO CameraDataUBO;
    DescriptorSet ObjectDescriptorSet;
    ObjectDataSSBO ObjectDataSSBO;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Run();
    void OnRender();
    void OnUpdate();

    void BeginFrame();
    void EndFrame();
    
    void Submit(const Scene& scene);
    void SortScene(Scene& scene);
    void Submit(const Mesh& mesh);
    void PushConstants(const Pipeline& pipeline, const void* pushConstants, const PushConstantDescription& description);
    
private:
    void Init();
    void ShutDown();

    void UpdateCamera();
    void UpdateScene();
    void LoadScene();

    const FrameContext& GetFrameContext() const;
    FrameContext& GetFrameContext();
    
private:
    GLFWwindow* m_Window;

    Device m_Device;
    Swapchain m_Swapchain;
    RenderPass m_RenderPass;
    std::vector<Framebuffer> m_Framebuffers;

    static constexpr u32 BUFFERED_FRAMES{2};
    u32 m_FrameNumber{0};
    u32 m_SwapchainImageIndex{0};

    std::vector<FrameContext> m_FrameContexts;
    FrameContext* m_CurrentFrameContext{nullptr};

    SceneDataUBO m_SceneDataUBO;
    
    Scene m_Scene;

    DescriptorPool m_DescriptorPool;
    DescriptorSetLayout m_GlobalDescriptorSetLayout;
    DescriptorSetLayout m_ObjectDescriptorSetLayout;
};

