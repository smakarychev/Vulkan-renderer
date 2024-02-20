#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <vector>

#include "ResourceUploader.h"
#include "VisibilityPass.h"
#include "Core/Camera.h"
#include "RenderPasses/RenderPassGeometry.h"
#include "RenderPasses/RenderPassGeometryCull.h"
#include "Rendering/CommandBuffer.h"
#include "Vulkan/Driver.h"
#include "Rendering/Swapchain.h"

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

struct CameraDataExtended
{
    glm::mat4 View;
    glm::mat4 Projection;
    glm::mat4 ViewProjection;
    glm::mat4 ViewProjectionInverse;
    glm::vec4 CameraPosition;
    glm::vec2 WindowSize;
    f32 FrustumNear;
    f32 FrustumFar;
};

struct CameraDataExtendedUBO
{
    Buffer Buffer;
    CameraDataExtended CameraData;
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

struct VisibilityBufferVisualizeData
{
    ShaderPipelineTemplate* Template;
    ShaderPipeline Pipeline;
    ShaderDescriptorSet DescriptorSet;
};

struct ComputeDepthPyramidData
{
    ShaderPipeline Pipeline;
    ShaderPipelineTemplate PipelineTemplate;
    std::unique_ptr<DepthPyramid> DepthPyramid;
};

struct FrameContext
{
    u32 CommandBufferIndex{0};
    
    SwapchainFrameSync FrameSync;
    u32 FrameNumber;

    glm::uvec2 Resolution;
    
    CommandBuffer Cmd;
    DeletionQueue DeletionQueue;
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

    template <typename Fn>
    void ImmediateUpload(Fn&& uploadFunction) const;

    GLFWwindow* GetWindow() { return m_Window; }
private:
    Renderer();
    void InitRenderingStructures();
    void InitDepthPyramidComputeStructures();
    void InitVisibilityPass();
    void InitVisibilityBufferVisualizationStructures();

    void Shutdown();
    void ShutdownVisibilityPass();

    void CreateDepthPyramid();
    void ComputeDepthPyramid();

    void SceneVisibilityPass();

    RenderingInfo GetColorRenderingInfo();

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
    CameraDataExtendedUBO m_CameraDataExtendedUBO;
    SceneDataUBO m_SceneDataUBO;
    
    ModelCollection m_ModelCollection;
    RenderPassGeometry m_OpaqueGeometry;
    RenderPassGeometryCull m_OpaqueGeometryCull;
    

    DescriptorAllocator m_PersistentDescriptorAllocator;
    DescriptorAllocator m_CullDescriptorAllocator;
    ResourceUploader m_ResourceUploader;

    ComputeDepthPyramidData m_ComputeDepthPyramidData;

    VisibilityPass m_VisibilityPass;
    // todo: temp object to visualize the visibility buffer
    VisibilityBufferVisualizeData m_VisibilityBufferVisualizeData;

    bool m_IsWindowResized{false};
    bool m_FrameEarlyExit{false};
};

template <typename Fn>
void Renderer::ImmediateUpload(Fn&& uploadFunction) const
{
    Driver::ImmediateUpload(uploadFunction);
}
