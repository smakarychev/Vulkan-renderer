﻿#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "Mesh.h"
#include "Scene.h"
#include "Vulkan/VulkanInclude.h"

#include <array>

#include "ResourceUploader.h"
#include "Settings.h"

class Camera;
class CameraController;

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

struct ObjectData
{
    glm::mat4 Transform;
};

struct ObjectDataSSBO
{
    Buffer Buffer;
    std::array<ObjectData, MAX_OBJECTS> Objects;
};

struct MaterialDataSSBO
{
    Buffer Buffer;
    std::array<MaterialBindless, MAX_OBJECTS> Materials;
};

struct ComputeDispatch
{
    ShaderPipeline* Pipeline;
    ShaderDescriptorSet* DescriptorSet;
    glm::uvec3 GroupSize;
};

struct BindlessData
{
    ShaderPipeline Pipeline;
    ShaderDescriptorSet DescriptorSet;
    BindlessDescriptorsState BindlessDescriptorsState;
};

struct FrameContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    SwapchainFrameSync FrameSync;
    u32 FrameNumber;
};

class Renderer
{
public:
    void Init();
    static Renderer* Get(); 
    ~Renderer();

    void Run();
    void OnRender();
    void OnUpdate();

    void BeginFrame();
    void EndFrame();

    void Dispatch(const ComputeDispatch& dispatch);
    
    void Submit(const Scene& scene);
    void SortScene(Scene& scene);
    void Submit(const Mesh& mesh);
    void PushConstants(const PipelineLayout& pipelineLayout, const void* pushConstants, const PushConstantDescription& description);

    template <typename Fn>
    void ImmediateUpload(Fn&& uploadFunction) const;

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void ShutDown();

    void OnWindowResize();
    void RecreateSwapchain();
    
    void UpdateCameraBuffers();
    void UpdateScene();
    void LoadScene();

    const FrameContext& GetFrameContext() const;
    FrameContext& GetFrameContext();
    
private:
    GLFWwindow* m_Window;
    std::unique_ptr<CameraController> m_CameraController;
    std::shared_ptr<Camera> m_Camera;

    Device m_Device;
    Swapchain m_Swapchain;
    RenderPass m_RenderPass;
    std::vector<Framebuffer> m_Framebuffers;
    
    u32 m_FrameNumber{0};
    u32 m_SwapchainImageIndex{0};

    std::vector<FrameContext> m_FrameContexts;
    FrameContext* m_CurrentFrameContext{nullptr};
    
    Buffer m_DrawIndirectBuffer;
    ObjectDataSSBO m_ObjectDataSSBO;
    CameraDataUBO m_CameraDataUBO;
    SceneDataUBO m_SceneDataUBO;
    MaterialDataSSBO m_MaterialDataSSBO;
    ShaderDescriptorSet m_GlobalObjectSet;
    
    Scene m_Scene;

    DescriptorAllocator m_PersistentDescriptorAllocator;
    DescriptorLayoutCache m_LayoutCache;
    ResourceUploader m_ResourceUploader;

    BindlessData m_BindlessData;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};

template <typename Fn>
void Renderer::ImmediateUpload(Fn&& uploadFunction) const
{
    Driver::ImmediateUpload(uploadFunction);
}