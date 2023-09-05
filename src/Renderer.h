#pragma once

#include "Vulkan/VulkanInclude.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Run();
    void OnRender();
    
private:
    void Init();
    void ShutDown();
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
};
