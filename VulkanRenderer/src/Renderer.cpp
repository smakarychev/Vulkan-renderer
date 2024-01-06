#include "Renderer.h"

#include <algorithm>
#include <volk.h>
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
    InitVisibilityBufferVisualizationStructures();

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
            InitVisibilityPass();
        }

        {
            TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Scene passes")
            if (m_ComputeDepthPyramidData.DepthPyramid->IsPendingTransition())
            {
                TimelineSemaphore& renderSemaphore = m_AsyncCullContext.RenderedSemaphore;
                m_ComputeDepthPyramidData.DepthPyramid->SubmitLayoutTransition(
                    GetFrameContext().GraphicsCommandBuffers.GetBuffer(), BufferSubmitTimelineSyncInfo{
                        .SignalSemaphores ={&renderSemaphore},
                        .SignalValues = {renderSemaphore.GetTimeline() + 1}});

                GetFrameContext().GraphicsCommandBuffers.NextIndex();
                GetFrameContext().GraphicsCommandBuffers.GetBuffer().Begin();
            }
            SceneVisibilityPass();
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
    m_CameraDataUBO.CameraData = {
        .View = m_Camera->GetView(),
        .Projection = m_Camera->GetProjection(),
        .ViewProjection = m_Camera->GetViewProjection()};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(
        m_CameraDataUBO.Buffer, &m_CameraDataUBO.CameraData, sizeof(CameraData), offsetBytes);

    FrustumPlanes frustumPlanes = m_Camera->GetFrustumPlanes();
    
    m_CameraDataExtendedUBO.CameraData = {
        .View = m_Camera->GetView(),
        .Projection = m_Camera->GetProjection(),
        .ViewProjection = m_Camera->GetViewProjection(),
        .ViewProjectionInverse = glm::inverse(m_Camera->GetViewProjection()),
        .WindowSize = {(f32)m_Swapchain.GetSize().x, (f32)m_Swapchain.GetSize().y},
        .FrustumNear = frustumPlanes.Near,
        .FrustumFar = frustumPlanes.Far};
    offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(
        m_CameraDataExtendedUBO.Buffer, &m_CameraDataExtendedUBO.CameraData, sizeof(CameraDataExtended), offsetBytes);
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

    GetFrameContext().GraphicsCommandBuffers.ResetBuffers();
    GetFrameContext().GraphicsCommandBuffers.SetIndex(0);
    CommandBuffer& cmd = GetFrameContext().GraphicsCommandBuffers.GetBuffer();
    cmd.Begin();

    GetFrameContext().ComputeCommandBuffers.ResetBuffers();
    GetFrameContext().ComputeCommandBuffers.SetIndex(0);
    CommandBuffer& computeCmd = GetFrameContext().ComputeCommandBuffers.GetBuffer();
    computeCmd.Begin();

    GetFrameContext().TracyProfilerBuffer.Reset();
    GetFrameContext().TracyProfilerBuffer.Begin();

    m_Swapchain.PrepareRendering(cmd);
    
    // todo: is this the best place for it?
    m_ResourceUploader.SubmitUpload();
}

RenderingInfo Renderer::GetClearRenderingInfo()
{
    // todo: fix: direct VKAPI Usage
    VkClearValue colorClear = {.color = {{0.05f, 0.05f, 0.05f, 1.0f}}};
    VkClearValue depthClear = {.depthStencil = {.depth = 0.0f}};
    
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetDrawImage().GetImageData(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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

    return renderingInfo;
}

RenderingInfo Renderer::GetLoadRenderingInfo()
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetDrawImage().GetImageData(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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

    return renderingInfo;
}

RenderingInfo Renderer::GetColorRenderingInfo()
{
    RenderingAttachment color = RenderingAttachment::Builder()
        .SetType(RenderingAttachmentType::Color)
        .FromImage(m_Swapchain.GetDrawImage().GetImageData(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .LoadStoreOperations(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
        .Build();

    RenderingInfo renderingInfo = RenderingInfo::Builder()
        .AddAttachment(color)
        .SetRenderArea(m_Swapchain.GetSize())
        .Build();

    return renderingInfo;
}

void Renderer::EndFrame()
{
    CommandBuffer& cmd = GetFrameContext().GraphicsCommandBuffers.GetBuffer();
    m_Swapchain.PreparePresent(cmd, m_SwapchainImageIndex);
    
    u32 frameNumber = GetFrameContext().FrameNumber;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;

    TracyVkCollect(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()))

    GetFrameContext().TracyProfilerBuffer.End();
    GetFrameContext().TracyProfilerBuffer.Submit(m_Device.GetQueues().Graphics, nullptr);
    
    cmd.End();

    cmd.Submit(m_Device.GetQueues().Graphics,
        BufferSubmitSyncInfo{
            .WaitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            .WaitSemaphores = {&sync.PresentSemaphore},
            .SignalSemaphores = {&sync.RenderSemaphore},
            .Fence = &sync.RenderFence});
    
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

    CommandBuffer& cmd = GetFrameContext().GraphicsCommandBuffers.GetBuffer();
    if (m_ComputeDepthPyramidData.DepthPyramid)
        m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    m_ComputeDepthPyramidData.DepthPyramid = std::make_unique<DepthPyramid>(m_Swapchain.GetDepthImage(), cmd,
        &m_ComputeDepthPyramidData);

    m_SceneCull.SetDepthPyramid(*m_ComputeDepthPyramidData.DepthPyramid);
}

void Renderer::ComputeDepthPyramid()
{
    ZoneScopedN("Compute depth pyramid");

    CommandBuffer& cmd = GetFrameContext().GraphicsCommandBuffers.GetBuffer();
    m_ComputeDepthPyramidData.DepthPyramid->Compute(m_Swapchain.GetDepthImage(), cmd);
}

void Renderer::SceneVisibilityPass()
{
    m_VisibilityPass.RenderVisibility({
        .Scene = &m_Scene,
        .SceneCull = &m_SceneCull,
        .FrameContext = &GetFrameContext(),
        .DepthPyramid = m_ComputeDepthPyramidData.DepthPyramid.get(),
        .DepthBuffer = &m_Swapchain.GetDepthImage()});

    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * GetFrameContext().FrameNumber);

    CommandBuffer& cmd = GetFrameContext().GraphicsCommandBuffers.GetBuffer();
    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());

    RenderCommand::BeginRendering(cmd, GetColorRenderingInfo());

    m_VisibilityBufferVisualizeData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_VisibilityBufferVisualizeData.Pipeline.GetPipelineLayout();
    m_VisibilityBufferVisualizeData.DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset});
    m_VisibilityBufferVisualizeData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    RenderCommand::Draw(cmd, 3);

    RenderCommand::EndRendering(cmd);
}

void Renderer::Submit(const Scene& scene)
{
    
}

void Renderer::SortScene(Scene& scene)
{
    std::sort(scene.GetRenderObjects().begin(), scene.GetRenderObjects().end(),
        [](const RenderObject& a, const RenderObject& b) { return a.Mesh < b.Mesh; });
}

void Renderer::InitRenderingStructures()
{
    VulkanCheck(volkInitialize(), "Failed to initialize volk");
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
        .AsyncCompute()
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

        CommandPool computePool = CommandPool::Builder()
            .SetQueue(QueueKind::Compute)
            .PerBufferReset(true)
            .Build();
        CommandBuffer computeBuffer = computePool.AllocateBuffer(CommandBufferKind::Primary);

        m_AsyncCullContext.CommandPool = computePool;
        m_AsyncCullContext.CommandBuffer = computeBuffer;
        m_AsyncCullContext.CulledSemaphore = TimelineSemaphore::Builder().Build();
        m_AsyncCullContext.RenderedSemaphore = TimelineSemaphore::Builder().Build();

        m_FrameContexts[i].FrameSync = m_Swapchain.GetFrameSync(i);
        m_FrameContexts[i].FrameNumber = i;
        m_FrameContexts[i].Resolution = m_Swapchain.GetSize();

        m_FrameContexts[i].TracyProfilerBuffer = pool.AllocateBuffer(CommandBufferKind::Primary);
    }

    std::array<CommandBuffer*, BUFFERED_FRAMES> cmds;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        cmds[i] = &m_FrameContexts[i].TracyProfilerBuffer;
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

    m_CameraDataExtendedUBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Uniform})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraDataExtended)) * BUFFERED_FRAMES)
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

void Renderer::InitVisibilityPass()
{
    bool recreated = m_VisibilityPass.Init({
        .Size = m_Swapchain.GetSize(),
        .Cmd = &GetFrameContext().ComputeCommandBuffers.GetBuffer(),
        .DescriptorAllocator = &m_PersistentDescriptorAllocator,
        .LayoutCache = &m_LayoutCache,
        .RenderingDetails = m_Swapchain.GetRenderingDetails(),
        .CameraBuffer = &m_CameraDataUBO.Buffer,
        .CommandsBuffer = &m_SceneCull.GetDrawCommands(),
        .ObjectsBuffer = &m_Scene.GetRenderObjectsBuffer(),
        .TrianglesBuffer = &m_SceneCull.GetTriangles(),
        .MaterialsBuffer = &m_Scene.GetMaterialsBuffer(),
        .Scene = &m_Scene});

    if (recreated)
        ShaderDescriptorSet::Destroy(m_VisibilityBufferVisualizeData.DescriptorSet);
    
    m_VisibilityBufferVisualizeData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_VisibilityBufferVisualizeData.Template)
        .AddBinding("u_visibility_texture", m_VisibilityPass.GetVisibilityImage().CreateDescriptorInfo(
            VK_FILTER_NEAREST, VK_IMAGE_LAYOUT_GENERAL))
        .AddBinding("u_camera_buffer", m_CameraDataExtendedUBO.Buffer, sizeof(CameraDataExtended), 0)
        .AddBinding("u_object_buffer", m_Scene.GetRenderObjectsBuffer())
        .AddBinding("u_positions_buffer", m_Scene.GetPositionsBuffer())
        .AddBinding("u_uv_buffer", m_Scene.GetUVBuffer())
        .AddBinding("u_indices_buffer", m_Scene.GetIndicesBuffer())
        .AddBinding("u_command_buffer", m_Scene.GetMeshletsIndirectBuffer())
        .AddBinding("u_material_buffer", m_Scene.GetMaterialsBuffer())
        .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
        .BuildManualLifetime();

    m_Scene.ApplyMaterialTextures(m_VisibilityBufferVisualizeData.DescriptorSet);
}

void Renderer::InitVisibilityBufferVisualizationStructures()
{
    m_VisibilityBufferVisualizeData.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/visibility-buffer/visualize-vert.shader",
        "../assets/shaders/processed/visibility-buffer/visualize-frag.shader"},
        "visualize-visibility-pipeline",
        m_PersistentDescriptorAllocator, m_LayoutCache);

    RenderingDetails renderingDetails = m_Swapchain.GetRenderingDetails();
    renderingDetails.DepthFormat = VK_FORMAT_UNDEFINED;
    
    m_VisibilityBufferVisualizeData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_VisibilityBufferVisualizeData.Template)
        .SetRenderingDetails(renderingDetails)
        .Build();
}

void Renderer::ShutDown()
{
    vkDeviceWaitIdle(Driver::DeviceHandle());

    m_Scene.OnShutdown();
    m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    Swapchain::Destroy(m_Swapchain);
    
    ShutDownVisibilityPass();
    m_SceneCull.Shutdown();
    m_ResourceUploader.ShutDown();
    ProfilerContext::Get()->ShutDown();
    Driver::Shutdown();
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::ShutDownVisibilityPass()
{
    m_VisibilityPass.ShutDown();
    ShaderDescriptorSet::Destroy(m_VisibilityBufferVisualizeData.DescriptorSet);
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

    Input::s_MainViewportSize = m_Swapchain.GetSize();
    m_Camera->SetViewport(m_Swapchain.GetSize().x, m_Swapchain.GetSize().y);
    for (auto& frameContext : m_FrameContexts)
        frameContext.Resolution = m_Swapchain.GetSize();
}

void Renderer::LoadScene()
{
    m_Scene.OnInit(&m_ResourceUploader);
    m_SceneCull.Init(m_Scene, m_CullDescriptorAllocator, m_LayoutCache);

    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    Model* mori = Model::LoadFromAsset("../assets/models/mori/mori.model");
    Model* gun = Model::LoadFromAsset("../assets/models/gun/scene.model");
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/FlightHelmet.model");
    Model* tree = Model::LoadFromAsset("../assets/models/tree/scene.model");
    Model* sphere = Model::LoadFromAsset("../assets/models/sphere/scene.model");
    Model* sphere_big = Model::LoadFromAsset("../assets/models/sphere_big/scene.model");
    Model* cube = Model::LoadFromAsset("../assets/models/real_cube/scene.model");
    Model* kitten = Model::LoadFromAsset("../assets/models/kitten/kitten.model");
   //Model* sponza = Model::LoadFromAsset("../assets/models/sponza/scene.model");
    
    m_Scene.AddModel(car, "car");
    m_Scene.AddModel(mori, "mori");
    m_Scene.AddModel(gun, "gun");
    m_Scene.AddModel(helmet, "helmet");
    m_Scene.AddModel(tree, "tree");
    m_Scene.AddModel(sphere, "sphere");
    m_Scene.AddModel(sphere_big, "sphere_big");
    m_Scene.AddModel(cube, "cube");
    m_Scene.AddModel(kitten, "kitten");

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
                model->CreateRenderObjects(&m_Scene, transform);

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
