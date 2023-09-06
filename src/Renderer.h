#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "Mesh.h"
#include "Vulkan/VulkanInclude.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Run();
    void OnRender();
    void OnUpdate();
    
    void Submit(const Mesh& mesh);
    void PushConstants(const void* pushConstants, const PushConstantDescription& description);
    
private:
    void Init();
    void ShutDown();

    void LoadModels();
    
private:
    GLFWwindow* m_Window;

    Device m_Device;
    Swapchain m_Swapchain;
    RenderPass m_RenderPass;
    std::vector<Framebuffer> m_Framebuffers;
    Pipeline m_Pipeline;

    CommandPool m_CommandPool;
    CommandBuffer m_CommandBuffer;

    Fence m_RenderFence;
    Semaphore m_RenderSemaphore;
    Semaphore m_PresentSemaphore;

    std::unique_ptr<Mesh> m_Mesh;
    PushConstantBuffer m_MeshPushConstants;
};

