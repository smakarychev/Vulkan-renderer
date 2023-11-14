#include "Renderer.h"

#include <algorithm>
#include <dinput.h>
#include <glm/ext/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "RenderObject.h"
#include "Scene.h"
#include "Model.h"
#include "VisibilityPass.h"
#include "Core/Input.h"
#include "Core/Random.h"
#include "GLFW/glfw3.h"
#include "Vulkan/DepthPyramid.h"
#include "Vulkan/RenderCommand.h"
#include "Vulkan/VulkanUtils.h"


Renderer::Renderer() = default;

void Renderer::Init()
{
    InitRenderingStructures();
    LoadScene();
    InitDepthPyramidComputeStructures();

    Input::s_MainViewportSize = m_Swapchain.GetSize();
    m_Camera = std::make_shared<Camera>();
    m_CameraController = std::make_unique<CameraController>(m_Camera);
}

Renderer* Renderer::Get()
{
    static Renderer renderer = {};
    return &renderer;
}

Renderer::~Renderer()
{
    ShutDown();
}

void Renderer::Run()
{
    while(!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();
        OnUpdate();
        OnRender();
    }
}

void Renderer::OnRender()
{
    ZoneScopedN("On render");

    BeginFrame();
    if (!m_FrameEarlyExit)
    {
        if (!m_ComputeDepthPyramidData.DepthPyramid)
        {
            CreateDepthPyramid();
            m_VisibilityPass.Init({
                .Size = m_Swapchain.GetSize(),
                .Cmd = &GetFrameContext().CommandBuffer,
                .Scene = &m_Scene,
                .DescriptorAllocator = &m_CullDescriptorAllocator,
                .LayoutCache = &m_LayoutCache,
                .RenderingDetails = m_Swapchain.GetRenderingDetails()});
        }

        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Scene passes")
            PrimaryScenePass();
            SecondaryScenePass(DisocclusionKind::Triangles);
            SecondaryScenePass(DisocclusionKind::Meshlets);
        }

        EndFrame();
        
        FrameMark; // tracy
    }
    else
    {
        m_FrameEarlyExit = false;
    }
}

void Renderer::OnUpdate()
{
    ZoneScopedN("On update");

    m_CameraController->OnUpdate(1.0f / 60.0f);
    //glm::vec3 pos = m_Camera->GetPosition();
    //glm::mat4 translation = glm::translate(glm::mat4(1.0f), 2.0f * glm::vec3(sinf(glfwGetTime() * 2), cosf(glfwGetTime() * 3), sinf(5 * glfwGetTime())));
    //m_Camera->SetPosition(translation * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    UpdateCameraBuffers();
    UpdateScene();
    UpdateComputeCullBuffers();
}

void Renderer::UpdateCameraBuffers()
{
    m_CameraDataUBO.CameraData = {.View = m_Camera->GetView(), .Projection = m_Camera->GetProjection(), .ViewProjection = m_Camera->GetViewProjection()};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_CameraDataUBO.Buffer, &m_CameraDataUBO.CameraData, sizeof(CameraData), offsetBytes);
}

void Renderer::UpdateComputeCullBuffers()
{
    m_SceneCull.UpdateBuffers(*m_Camera, m_ResourceUploader, GetFrameContext());
}

void Renderer::UpdateScene()
{
    // todo: this whole function is strange, and should most certainly be a part of `Scene` class
    if (m_Scene.IsDirty())
    {
        SortScene(m_Scene);
        m_Scene.CreateSharedMeshContext();
        
        m_Scene.OnUpdate(1.0f / 60.0f);
        m_Scene.ClearDirty();
    }
    

    FrameContext& context = GetFrameContext();
    
    f32 freq = (f32)glfwGetTime() / 10.0f;
    f32 red = (sin(freq) + 1.0f) * 0.5f;
    f32 green = (cos(freq) + 1.0f) * 0.5f;
    f32 blue = (red + green) * 0.5f;
    f32 sunFreq = (f32)glfwGetTime();
    f32 sunPos = sin(sunFreq);
    m_SceneDataUBO.SceneData.SunlightDirection = {sunPos * 2.0f, (sunPos + 2.0f) * 10.0f, sunPos * 8.0f, 1.0f};
    m_SceneDataUBO.SceneData.SunlightColor = {0.8f, 0.1f, 0.1f, 1.0};
    m_SceneDataUBO.SceneData.FogColor = {0.3f, 0.1f, 0.1f, 1.0f};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * context.FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_SceneDataUBO.Buffer, &m_SceneDataUBO.SceneData, sizeof(SceneData), offsetBytes);
}

void Renderer::BeginFrame()
{
    ZoneScopedN("Begin frame");

    u32 frameNumber = GetFrameContext().FrameNumber;
    m_SwapchainImageIndex = m_Swapchain.AcquireImage(frameNumber);
    if (m_SwapchainImageIndex == INVALID_SWAPCHAIN_IMAGE)
    {
        m_FrameEarlyExit = true;
        RecreateSwapchain();
        return;
    }

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    cmd.Reset();
    cmd.Begin();

    m_Swapchain.PrepareRendering(cmd, m_SwapchainImageIndex);
    
    // todo: is this the best place for it?
    m_ResourceUploader.SubmitUpload();
}

void Renderer::PrimaryScenePass()
{
    ZoneScopedN("Primary Scene Pass");
    const CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    CullCompute(m_Scene);
    
    VkClearValue colorClear = {.color = {{0.05f, 0.05f, 0.05f, 1.0f}}};
    VkClearValue depthClear = {.depthStencil = {.depth = 0.0f}};
    
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetColorImage(m_SwapchainImageIndex).GetImageData(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .ClearValue(colorClear)
        .Build();

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(m_Swapchain.GetDepthImage().GetImageData(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .ClearValue(depthClear)
        .Build();

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(m_Swapchain.GetSize())
        .Build();

    RenderCommand::BeginRendering(cmd, renderingInfo);
    
    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());

    Submit(m_Scene);

    RenderCommand::EndRendering(cmd);

    ComputeDepthPyramid();
}

void Renderer::SecondaryScenePass(DisocclusionKind disocclusionKind)
{
    ZoneScopedN("Secondary Scene Pass");
    const CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    SecondaryCullCompute(m_Scene, disocclusionKind);

    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetColorImage(m_SwapchainImageIndex).GetImageData(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
        .Build();

    RenderingAttachment depth = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Depth)
        .FromImage(m_Swapchain.GetDepthImage().GetImageData(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
        .Build();

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .AddAttachment(depth)
        .SetRenderArea(m_Swapchain.GetSize())
        .Build();

    RenderCommand::BeginRendering(cmd, renderingInfo);
    
    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());

    Submit(m_Scene);

    RenderCommand::EndRendering(cmd);

    ComputeDepthPyramid();
}

void Renderer::EndFrame()
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    m_Swapchain.PreparePresent(cmd, m_SwapchainImageIndex);
    
    u32 frameNumber = GetFrameContext().FrameNumber;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;
    
    TracyVkCollect(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()))
    
    cmd.End();

    RenderCommand::SubmitCommandBuffer(cmd, m_Device.GetQueues().Graphics,
        {
            .WaitSemaphores = {&sync.PresentSemaphore},
            .SignalSemaphores = {&sync.RenderSemaphore},
            .WaitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            .Fence = &sync.RenderFence 
        });
    
    bool isFramePresentSuccessful = m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, m_SwapchainImageIndex, frameNumber); 
    bool shouldRecreateSwapchain = m_IsWindowResized || !isFramePresentSuccessful;
    if (shouldRecreateSwapchain)
        RecreateSwapchain();
    
    m_FrameNumber++;
    m_CurrentFrameContext = &m_FrameContexts[m_FrameNumber % BUFFERED_FRAMES];
    ProfilerContext::Get()->NextFrame();
    
    // todo: is this the best place for it?
    m_ResourceUploader.StartRecording();
}

void Renderer::Dispatch(const ComputeDispatch& dispatch)
{
    /*CommandBuffer& cmd = GetFrameContext().ComputeCommandBuffer;
    
    dispatch.Pipeline->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    u32 computeUboOffset = (u32)(vkUtils::alignUniformBufferSizeBytes(sizeof(f32) * GetFrameContext().FrameNumber));
    dispatch.DescriptorSet->Bind(cmd, DescriptorKind::Global, dispatch.Pipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {computeUboOffset});
    RenderCommand::Dispatch(cmd, dispatch.GroupSize);*/
}

void Renderer::CreateDepthPyramid()
{
    ZoneScopedN("Create depth pyramid");

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    if (m_ComputeDepthPyramidData.DepthPyramid)
        m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    m_ComputeDepthPyramidData.DepthPyramid = std::make_unique<DepthPyramid>(m_Swapchain.GetDepthImage(), cmd,
        &m_ComputeDepthPyramidData);

    m_SceneCull.SetDepthPyramid(*m_ComputeDepthPyramidData.DepthPyramid);
}

void Renderer::ComputeDepthPyramid()
{
    ZoneScopedN("Compute depth pyramid");
    
     u32 thrice = 3;
    if (thrice)
    {
        CommandBuffer& cmd = GetFrameContext().CommandBuffer;
        m_ComputeDepthPyramidData.DepthPyramid->ComputePyramid(m_Swapchain.GetDepthImage(), cmd);
        thrice--;
    }
}

void Renderer::SceneVisibilityPass()
{
    
}

void Renderer::CullCompute(const Scene& scene)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Compute cull")
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Reset cull buffers")
        m_SceneCull.ResetCullBuffers(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Mesh cull compute")
        ZoneScopedN("Mesh cull compute");
        m_SceneCull.PerformMeshCulling(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Mesh compact compute")
        ZoneScopedN("Mesh compact compute");
        m_SceneCull.PerformMeshCompaction(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Meshlet cull compute")
        ZoneScopedN("Meshlet cull compute");
        m_SceneCull.PerformMeshletCulling(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Meshlet compact compute")
        ZoneScopedN("Meshlet compact compute");
        m_SceneCull.PerformMeshletCompaction(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull clear command buffer compute")
        ZoneScopedN("Triangle cull clear command buffer compute");
        m_SceneCull.ClearTriangleCullCommandBuffer(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull compact compute")
        ZoneScopedN("Triangle cull compact compute");
        m_SceneCull.PerformTriangleCullingCompaction(GetFrameContext());
    }
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Final compact compute")
        ZoneScopedN("Final compact compute");
        m_SceneCull.PerformFinalCompaction(GetFrameContext());
    }
}

void Renderer::SecondaryCullCompute(const Scene& scene, DisocclusionKind disocclusionKind)
{
    
    if ((disocclusionKind & DisocclusionKind::Triangles) == DisocclusionKind::Triangles)
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Secondary compute cull triangles")
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Reset secondary cull buffers")
            m_SceneCull.ResetSecondaryCullBuffers(GetFrameContext(), 0);
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull clear command buffer compute")
            ZoneScopedN("Triangle cull clear command buffer compute");
            m_SceneCull.ClearTriangleCullCommandBufferSecondary(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull compact compute")
            ZoneScopedN("Triangle cull compact compute");
            m_SceneCull.PerformSecondaryTriangleCullingCompaction(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Final compact compute")
            ZoneScopedN("Final compact compute");
            m_SceneCull.PerformFinalCompaction(GetFrameContext());
        }
    }
    if ((disocclusionKind & DisocclusionKind::Meshlets) == DisocclusionKind::Meshlets)
    {
        TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Secondary compute cull meshlets")
        if ((disocclusionKind & DisocclusionKind::Triangles) == DisocclusionKind::Triangles)
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Reset secondary cull buffers")
            m_SceneCull.ResetSecondaryCullBuffers(GetFrameContext(), 1);
        }
        else
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Reset secondary cull buffers")
            m_SceneCull.ResetSecondaryCullBuffers(GetFrameContext(), 2);
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Mesh cull compute")
            ZoneScopedN("Mesh cull compute");
            m_SceneCull.PerformSecondaryMeshCulling(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Mesh compact compute")
            ZoneScopedN("Mesh compact compute");
            m_SceneCull.PerformSecondaryMeshCompaction(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Meshlet cull compute")
            ZoneScopedN("Meshlet cull compute");
            m_SceneCull.PerformSecondaryMeshletCulling(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Meshlet compact compute")
            ZoneScopedN("Meshlet compact compute");
            m_SceneCull.PerformSecondaryMeshletCompaction(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull clear command buffer compute")
            ZoneScopedN("Triangle cull clear command buffer compute");
            m_SceneCull.ClearTriangleCullCommandBuffer(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Triangle cull compact compute")
            ZoneScopedN("Triangle cull compact compute");
            m_SceneCull.PerformTertiaryTriangleCullingCompaction(GetFrameContext());
        }
        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Final compact compute")
            ZoneScopedN("Final compact compute");
            m_SceneCull.PerformFinalCompaction(GetFrameContext());
        }
    }
}

void Renderer::Submit(const Scene& scene)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Scene render")
    ZoneScopedN("Scene render");

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;

    m_BindlessData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_BindlessData.Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber);
    u32 sceneDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset, sceneDataOffset});
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Material, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * GetFrameContext().FrameNumber;
    scene.Bind(cmd);
    
    RenderCommand::DrawIndexedIndirectCount(cmd,
       scene.GetMeshletsIndirectFinalBuffer(), 0,
       m_SceneCull.GetVisibleMeshletsBuffer(), offset,
       scene.GetMeshletCount(), sizeof(VkDrawIndexedIndirectCommand));

    //RenderCommand::DrawIndexedIndirect(cmd, scene.GetMeshletsIndirectBuffer(), 0, scene.GetMeshletCount(), sizeof(VkDrawIndexedIndirectCommand));
}

void Renderer::SortScene(Scene& scene)
{
    std::sort(scene.GetRenderObjects().begin(), scene.GetRenderObjects().end(),
        [](const RenderObject& a, const RenderObject& b) { return a.Mesh < b.Mesh; });
}

void Renderer::PushConstants(const PipelineLayout& pipelineLayout, const void* pushConstants, const PushConstantDescription& description)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    RenderCommand::PushConstants(cmd, pipelineLayout, pushConstants, description);
}

void Renderer::InitRenderingStructures()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(1600, 900, "My window", nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, i32 width, i32 height)
    {
        Renderer* renderer = (Renderer*)glfwGetWindowUserPointer(window);
        renderer->OnWindowResize();
    });

    m_Device = Device::Builder()
        .Defaults()
        .SetWindow(m_Window)
        .Build();

    Driver::Init(m_Device);

    m_ResourceUploader.Init();
    
    m_Swapchain = Swapchain::Builder()
        .DefaultHints()
        .FromDetails(m_Device.GetSurfaceDetails())
        .SetDevice(m_Device)
        .BufferedFrames(BUFFERED_FRAMES)
        .BuildManualLifetime();

    m_FrameContexts.resize(BUFFERED_FRAMES);
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        CommandPool pool = CommandPool::Builder()
            .SetQueue(QueueKind::Graphics)
            .PerBufferReset(true)
            .Build();
        CommandBuffer buffer = pool.AllocateBuffer(CommandBufferKind::Primary);

        CommandPool computePool = CommandPool::Builder()
            .SetQueue(QueueKind::Graphics)
            .PerBufferReset(true)
            .Build();
        CommandBuffer computeBuffer = computePool.AllocateBuffer(CommandBufferKind::Primary);

        m_FrameContexts[i].CommandPool = pool;
        m_FrameContexts[i].CommandBuffer = buffer;
        m_FrameContexts[i].FrameSync = m_Swapchain.GetFrameSync(i);
        m_FrameContexts[i].FrameNumber = i;
        m_FrameContexts[i].Resolution = m_Swapchain.GetSize();
    }

    std::array<CommandBuffer*, BUFFERED_FRAMES> cmds;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        cmds[i] = &m_FrameContexts[i].CommandBuffer;
    ProfilerContext::Get()->Init(cmds);

    // descriptors
    m_PersistentDescriptorAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100000)
        .Build();

    m_CullDescriptorAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(100)
        .Build();

    m_SceneDataUBO.Buffer = Buffer::Builder()
        .SetKind(BufferKind::Uniform)
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * BUFFERED_FRAMES)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();

    m_CameraDataUBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Uniform})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * BUFFERED_FRAMES)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();

    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::InitDepthPyramidComputeStructures()
{
    Shader* computeDepthPyramid = Shader::ReflectFrom({"../assets/shaders/processed/culling/depth-pyramid-comp.shader"});

    m_ComputeDepthPyramidData.PipelineTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_CullDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeDepthPyramid)
        .Build();

    m_ComputeDepthPyramidData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(&m_ComputeDepthPyramidData.PipelineTemplate)
        .Build();
}

void Renderer::ShutDown()
{
    vkDeviceWaitIdle(Driver::DeviceHandle());

    m_Scene.OnShutdown();
    m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    Swapchain::Destroy(m_Swapchain);
    
    m_VisibilityPass.ShutDown();
    m_SceneCull.ShutDown();
    m_ResourceUploader.ShutDown();
    ProfilerContext::Get()->ShutDown();
    Driver::Shutdown();
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::OnWindowResize()
{
    m_IsWindowResized = true;
}

void Renderer::RecreateSwapchain()
{
    m_IsWindowResized = false;    
    i32 width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }
    
    vkDeviceWaitIdle(Driver::DeviceHandle());

    Swapchain::Builder newSwapchainBuilder = Swapchain::Builder()
        .DefaultHints()
        .FromDetails(m_Device.GetSurfaceDetails())
        .SetDevice(m_Device)
        .BufferedFrames(BUFFERED_FRAMES)
        .SetSyncStructures(m_Swapchain.GetFrameSync());
    
    Swapchain::Destroy(m_Swapchain);
    
    m_Swapchain = newSwapchainBuilder.BuildManualLifetime();
    m_ComputeDepthPyramidData.DepthPyramid.reset();
    m_VisibilityPass.ShutDown();

    Input::s_MainViewportSize = m_Swapchain.GetSize();
    m_Camera->SetViewport(m_Swapchain.GetSize().x, m_Swapchain.GetSize().y);
    for (auto& frameContext : m_FrameContexts)
        frameContext.Resolution = m_Swapchain.GetSize();
}

void Renderer::LoadScene()
{
    m_Scene.OnInit(&m_ResourceUploader);
    m_SceneCull.Init(m_Scene, m_CullDescriptorAllocator, m_LayoutCache);
    
    ShaderPipelineTemplate::Builder templateBuilder = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache);
    
    Shader* bindlessShaderReflection = Shader::ReflectFrom({
    "../assets/shaders/processed/bindless-textures-test-vert.shader",
    "../assets/shaders/processed/bindless-textures-test-frag.shader"});

    ShaderPipelineTemplate bindlessTemplate = templateBuilder
        .SetShaderReflection(bindlessShaderReflection)
        .Build();

    m_Scene.AddShaderTemplate(bindlessTemplate, "bindless");

    m_BindlessData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("bindless"))
        .CompatibleWithVertex(VertexP3N3UV2::GetInputDescriptionDI())
        .SetRenderingDetails(m_Swapchain.GetRenderingDetails())
        .Build();

    m_BindlessData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("bindless"))
        .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
        .AddBinding("u_camera_buffer", m_CameraDataUBO.Buffer, sizeof(CameraData), 0)
        .AddBinding("u_scene_data", m_SceneDataUBO.Buffer, sizeof(SceneData), 0)
        .AddBinding("u_object_buffer", m_Scene.GetRenderObjectsBuffer())
        .AddBinding("u_material_buffer", m_Scene.GetMaterialsBuffer())
        .AddBinding("u_meshlet_buffer", m_Scene.GetMeshletsBuffer())
        .Build();
    

    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    Model* mori = Model::LoadFromAsset("../assets/models/mori/mori.model");
    Model* gun = Model::LoadFromAsset("../assets/models/gun/scene.model");
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/FlightHelmet.model");
    Model* tree = Model::LoadFromAsset("../assets/models/tree/scene.model");
    Model* sphere = Model::LoadFromAsset("../assets/models/sphere/scene.model");
    Model* sphere_big = Model::LoadFromAsset("../assets/models/sphere_big/scene.model");
   //Model* sponza = Model::LoadFromAsset("../assets/models/sponza/scene.model");
    
    m_Scene.AddModel(car, "car");
    m_Scene.AddModel(mori, "mori");
    m_Scene.AddModel(gun, "gun");
    m_Scene.AddModel(helmet, "helmet");
    m_Scene.AddModel(tree, "tree");
    m_Scene.AddModel(sphere, "sphere");
    m_Scene.AddModel(sphere_big, "sphere_big");

    //m_Scene.AddModel(sponza, "sponza");

    std::vector models = {"car", "helmet", "mori", "gun", "sphere_big"};

    for (i32 x = -6; x <= 6; x++)
    {
        for (i32 y = -3; y <= 3; y++)
        {
            for (i32 z = -6; z <= 6; z++)
            {
                u32 modelIndex = Random::UInt32(0, (u32)models.size() - 1);
            
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(x * 3.0f, y * 1.5f, z * 3.0f)) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

                Model* model = m_Scene.GetModel(models[modelIndex]);
                model->CreateRenderObjects(&m_Scene, transform, m_BindlessData.DescriptorSet, m_BindlessData.BindlessDescriptorsState);

                //model->CreateDebugBoundingSpheres(&m_Scene, transform, m_BindlessData.DescriptorSet, m_BindlessData.BindlessDescriptorsState);
            }
        }
    }
}

const FrameContext& Renderer::GetFrameContext() const
{
    return *m_CurrentFrameContext;
}

FrameContext& Renderer::GetFrameContext()
{
    return *m_CurrentFrameContext;
}
