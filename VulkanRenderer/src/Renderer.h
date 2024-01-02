#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "Mesh.h"
#include "Scene.h"
#include "Vulkan/VulkanInclude.h"

#include <array>
#include <vector>

#include "ResourceUploader.h"
#include "SceneCull.h"
#include "Settings.h"
#include "VisibilityPass.h"
#include "Core/Camera.h"
#include "Core/ProfilerContext.h"

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

struct ComputeDepthPyramidData
{
    ShaderPipeline Pipeline;
    ShaderPipelineTemplate PipelineTemplate;
    std::unique_ptr<DepthPyramid> DepthPyramid;
};

struct AsyncCullContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;

    TimelineSemaphore CulledSemaphore;
    TimelineSemaphore RenderedSemaphore;
};

struct FrameContext
{
    AsyncCullContext AsyncCullContext;

    CommandBufferArray GraphicsCommandBuffers{QueueKind::Graphics, false};
    CommandBufferArray ComputeCommandBuffers{QueueKind::Compute, false};
    
    CommandPool CommandPool;
    std::vector<CommandBuffer> CommandBuffers;
    u32 CommandBufferIndex{0};
    
    SwapchainFrameSync FrameSync;
    u32 FrameNumber;

    glm::uvec2 Resolution;
    
    CommandBuffer TracyProfilerBuffer;
};

enum class DisocclusionKind { Triangles = BIT(1), Meshlets = BIT(2) };
CREATE_ENUM_FLAGS_OPERATORS(DisocclusionKind)

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

    template <typename Fn>
    void ImmediateUpload(Fn&& uploadFunction) const;

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void InitDepthPyramidComputeStructures();
    void ShutDown();

    void CreateDepthPyramid();
    void ComputeDepthPyramid();

    void SceneVisibilityPass();
    void PrimaryScenePass();

    RenderingInfo GetClearRenderingInfo();
    RenderingInfo GetLoadRenderingInfo();

    void OnWindowResize();
    void RecreateSwapchain();
    
    void UpdateCameraBuffers();
    void UpdateComputeCullBuffers();
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
    
    u32 m_FrameNumber{0};
    u32 m_SwapchainImageIndex{0};

    std::vector<FrameContext> m_FrameContexts;
    FrameContext* m_CurrentFrameContext{nullptr};

    CameraDataUBO m_CameraDataUBO;
    SceneDataUBO m_SceneDataUBO;
    
    Scene m_Scene;

    DescriptorAllocator m_PersistentDescriptorAllocator;
    DescriptorAllocator m_CullDescriptorAllocator;
    DescriptorLayoutCache m_LayoutCache;
    ResourceUploader m_ResourceUploader;

    BindlessData m_BindlessData;
    ComputeDepthPyramidData m_ComputeDepthPyramidData;

    VisibilityPass m_VisibilityPass;

    SceneCull m_SceneCull;
    u32 m_CullBatchCount{0};

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};

template <typename Fn>
void Renderer::ImmediateUpload(Fn&& uploadFunction) const
{
    Driver::ImmediateUpload(uploadFunction);
}
