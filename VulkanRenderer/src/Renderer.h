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

    void CreateDepthPyramid();
    void ComputeDepthPyramid();
    void CullCompute(const Scene& scene);
    void SecondaryCullCompute(const Scene& scene);
    void Submit(const Scene& scene);
    void SortScene(Scene& scene);
    void PushConstants(const PipelineLayout& pipelineLayout, const void* pushConstants, const PushConstantDescription& description);

    template <typename Fn>
    void ImmediateUpload(Fn&& uploadFunction) const;

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void InitDepthPyramidComputeStructures();
    void ShutDown();

    void PrimaryScenePass();
    void SecondaryScenePass();

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

    bool m_DepthPyramidIsPresent{false};

    SceneCull m_SceneCull;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};

template <typename Fn>
void Renderer::ImmediateUpload(Fn&& uploadFunction) const
{
    Driver::ImmediateUpload(uploadFunction);
}
