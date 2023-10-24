#include "Renderer.h"

#include <algorithm>
#include <dinput.h>
#include <glm/ext/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "RenderObject.h"
#include "Scene.h"
#include "Model.h"
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
    InitCullComputeStructures();
    InitDepthPyramidComputeStructures();
    InitReprojectionComputeStructures();
    InitDilateComputeStructures();
    InitCompactComputeStructures();

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
    BeginFrame();
    if (!m_FrameEarlyExit)
    {
        if (!m_ComputeDepthPyramidData.DepthPyramid)
            CreateDepthPyramid();
        else
            ComputeDepthPyramid();

        CullCompute(m_Scene);
        CompactCompute(m_Scene);

        // todo: remove me, please
        BeginGraphics();
        
        Submit(m_Scene);

        // todo: remove me, please
        EndGraphics();
        
        
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
    m_CameraController->OnUpdate(1.0f / 60.0f);
    //glm::vec3 pos = m_Camera->GetPosition();
    //glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(2.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    //m_Camera->SetPosition(rot * glm::vec4(m_Camera->GetPosition(), 1.0f));
    UpdateCameraBuffers();
    UpdateScene();
    UpdateComputeCullBuffers();
    UpdateComputeReprojectionBuffers();
    UpdateComputeCompactBuffers();
}

void Renderer::UpdateCameraBuffers()
{
    m_CameraDataUBO.CameraData = {.View = m_Camera->GetView(), .Projection = m_Camera->GetProjection(), .ViewProjection = m_Camera->GetViewProjection()};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_CameraDataUBO.Buffer, &m_CameraDataUBO.CameraData, sizeof(CameraData), offsetBytes);
}

void Renderer::UpdateComputeCullBuffers()
{
    glm::mat4 view = m_Camera->GetView();
    FrustumPlanes planes = m_Camera->GetFrustumPlanes();
    ProjectionData projectionData = m_Camera->GetProjectionData();
    auto& sceneData = m_ComputeCullData.SceneDataUBO.SceneData; 
    sceneData.ViewMatrix = view;
    sceneData.FrustumPlanes = planes; 
    sceneData.ProjectionData = projectionData; 
    sceneData.TotalMeshCount = (u32)m_Scene.GetRenderObjects().size();
    if (m_ComputeDepthPyramidData.DepthPyramid)
    {
        sceneData.PyramidWidth = (f32)m_ComputeDepthPyramidData.DepthPyramid->GetTexture().GetImageData().Width; 
        sceneData.PyramidHeight = (f32)m_ComputeDepthPyramidData.DepthPyramid->GetTexture().GetImageData().Height;
    }
    
    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(sceneData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_ComputeCullData.SceneDataUBO.Buffer, &sceneData,
        sizeof(sceneData), offset);
}

void Renderer::UpdateComputeReprojectionBuffers()
{
    auto& reprojectionData = m_ComputeReprojectionData.ReprojectionUBO.ReprojectionData; 
    reprojectionData.LastViewInverse = glm::inverse(reprojectionData.View);
    reprojectionData.LastProjectionInverse = glm::inverse(reprojectionData.Projection);
    reprojectionData.Projection = m_Camera->GetProjection();
    reprojectionData.View = m_Camera->GetView();

    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(reprojectionData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_ComputeReprojectionData.ReprojectionUBO.Buffer, &reprojectionData,
        sizeof(reprojectionData), offset);
}

void Renderer::UpdateComputeCompactBuffers()
{
    auto& compactionData = m_ComputeCompactData.CompactUBO.CompactionData;
    compactionData.DrawCount = 0;

    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(compactionData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_ComputeCompactData.CompactUBO.Buffer, &compactionData,
       sizeof(compactionData), offset);
}

void Renderer::UpdateScene()
{
    // todo: this whole function is strange, and should most certainly be a part of `Scene` class
    if (m_Scene.IsDirty())
    {
        SortScene(m_Scene);
        m_Scene.CreateSharedMeshContext(m_ResourceUploader);
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

    // assuming that object transform can change
    for (u32 i = 0; i < m_Scene.GetRenderObjects().size(); i++)
    {
        m_ObjectDataSSBO.Objects[i].Transform = m_Scene.GetRenderObjects()[i].Transform;
        MaterialGPU& material = m_Scene.GetMaterialGPU(m_Scene.GetRenderObjects()[i].MaterialGPU);
        m_MaterialDataSSBO.Materials[i].Albedo = material.Albedo;
        m_MaterialDataSSBO.Materials[i].AlbedoTextureHandle = material.AlbedoTextureHandle;
    }

    m_ResourceUploader.UpdateBuffer(m_ObjectDataSSBO.Buffer, m_ObjectDataSSBO.Objects.data(), m_ObjectDataSSBO.Objects.size() * sizeof(ObjectData), 0);
    m_ResourceUploader.UpdateBuffer(m_MaterialDataSSBO.Buffer, m_MaterialDataSSBO.Materials.data(), m_MaterialDataSSBO.Materials.size() * sizeof(MaterialGPU), 0);
}

void Renderer::BeginFrame()
{
    u32 frameNumber = GetFrameContext().FrameNumber;
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    m_SwapchainImageIndex = m_Swapchain.AcquireImage(frameNumber);
    if (m_SwapchainImageIndex == INVALID_SWAPCHAIN_IMAGE)
    {
        m_FrameEarlyExit = true;
        RecreateSwapchain();
        return;
    }

    cmd.Reset();
    cmd.Begin();
    
    // todo: is this the best place for it?
    m_ResourceUploader.SubmitUpload();
}

void Renderer::BeginGraphics()
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    VkClearValue colorClear = {.color = {{0.05f, 0.05f, 0.05f, 1.0f}}};
    VkClearValue depthClear = {.depthStencil = {.depth = 0.0f}};
    m_RenderPass.Begin(cmd, m_Framebuffers[m_SwapchainImageIndex], {colorClear, depthClear});

    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());
}

void Renderer::EndGraphics()
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    m_RenderPass.End(cmd);
}

void Renderer::EndFrame()
{
    u32 frameNumber = GetFrameContext().FrameNumber;
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;
    
    TracyVkCollect(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()))
    
    cmd.End();
    cmd.Submit(m_Device.GetQueues().Graphics,
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
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    if (m_ComputeDepthPyramidData.DepthPyramid)
    {
        ShaderDescriptorSet::Destroy(m_ComputeCullData.DescriptorSet);
        m_ComputeDepthPyramidData.DepthPyramid.reset();
    }
    
    m_ComputeDepthPyramidData.DepthPyramid = std::make_unique<DepthPyramid>(m_Swapchain.GetDepthImage(), cmd,
        &m_ComputeDepthPyramidData, &m_ComputeReprojectionData, &m_ComputeDilateData);

    const Texture& pyramid = m_ComputeDepthPyramidData.DepthPyramid->GetTexture();

    m_ComputeCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("compute-cull-template"))
        .AddBinding("u_object_buffer", m_Scene.GetRenderObjectsBuffer(), m_Scene.GetRenderObjectsBuffer().GetSizeBytes(), 0)
        .AddBinding("u_command_buffer", m_Scene.GetIndirectBuffer(), m_Scene.GetIndirectBuffer().GetSizeBytes(), 0)
        .AddBinding("u_scene_data", m_ComputeCullData.SceneDataUBO.Buffer, vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeFrustumCullData::SceneDataUBO::Data)), 0)
        .AddBinding("u_depth_pyramid", {
            .View = pyramid.GetImageData().View,
            .Sampler = m_ComputeDepthPyramidData.DepthPyramid->GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();
}

void Renderer::ComputeDepthPyramid()
{
    bool once = true;
    if (once)
    {
        CommandBuffer& cmd = GetFrameContext().CommandBuffer;
        m_ComputeDepthPyramidData.DepthPyramid->ComputePyramid(m_Swapchain.GetDepthImage(), cmd);
        once = false;
    }
    
}

void Renderer::CullCompute(const Scene& scene)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Compute cull")

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    u32 offset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeFrustumCullData::SceneDataUBO::Data)) * GetFrameContext().FrameNumber;
    
    m_ComputeCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ComputeCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ComputeCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset});
    RenderCommand::Dispatch(cmd, {m_Scene.GetRenderObjects().size() / 64 + 1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &m_Device.GetQueues().Graphics,
        .Buffer = &scene.GetIndirectBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void Renderer::CompactCompute(const Scene& scene)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Compact compute")

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    u32 offset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCompactData::CompactUBO::Data)) * GetFrameContext().FrameNumber;

    PushConstantDescription pushConstantDescription = PushConstantDescription::Builder()
        .SetStages(VK_SHADER_STAGE_COMPUTE_BIT)
        .SetSizeBytes(sizeof(u32))
        .Build();
    m_ComputeCompactData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ComputeCompactData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ComputeCompactData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset});
    u32 objectsCount = (u32)m_Scene.GetRenderObjects().size();
    RenderCommand::PushConstants(cmd, m_ComputeCompactData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {m_Scene.GetRenderObjects().size() / 64 + 1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &m_Device.GetQueues().Graphics,
        .Buffer = &scene.GetIndirectCompactBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void Renderer::Submit(const Scene& scene)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Scene render")

    CommandBuffer& cmd = GetFrameContext().CommandBuffer;

    m_BindlessData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_BindlessData.Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber);
    u32 sceneDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset, sceneDataOffset});
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Material, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCompactData::CompactUBO::Data)) * GetFrameContext().FrameNumber;
    scene.Bind(cmd);
    
    RenderCommand::DrawIndexedIndirectCount(cmd,
       scene.GetIndirectCompactBuffer(), 0,
       m_ComputeCompactData.CompactUBO.Buffer, offset,
       (u32)scene.GetRenderObjects().size(), sizeof(VkDrawIndexedIndirectCommand));
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

    std::vector<AttachmentTemplate> attachmentTemplates = m_Swapchain.GetAttachmentTemplates();
    
    Subpass subpass = Subpass::Builder()
        .SetAttachments(attachmentTemplates)
        .Build();

    m_RenderPass = RenderPass::Builder()
        .AddSubpass(subpass)
        .AddSubpassDependency(
            VK_SUBPASS_EXTERNAL,
            subpass,
            {
                .SourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .SourceAccessMask = 0,
                .DestinationAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            })
        .AddSubpassDependency(
            VK_SUBPASS_EXTERNAL,
            subpass,
            {
                .SourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .SourceAccessMask = 0,
                .DestinationAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
            })
        .Build();

    m_Framebuffers = m_Swapchain.GetFramebuffers(m_RenderPass);

    m_FrameContexts.resize(BUFFERED_FRAMES);
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        CommandPool pool = CommandPool::Builder()
            .SetQueue(QueueKind::Graphics)
            .PerBufferReset(true)
            .Build();
        CommandBuffer buffer = pool.AllocateBuffer(CommandBufferKind::Primary);

        m_FrameContexts[i].CommandPool = pool;
        m_FrameContexts[i].CommandBuffer = buffer;
        m_FrameContexts[i].FrameSync = m_Swapchain.GetFrameSync(i);
        m_FrameContexts[i].FrameNumber = i;
    }

    std::array<CommandBuffer*, BUFFERED_FRAMES> cmds;
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        cmds[i] = &m_FrameContexts[i].CommandBuffer;
    ProfilerContext::Get()->Init(cmds);

    // descriptors
    m_PersistentDescriptorAllocator = DescriptorAllocator::Builder()
        .SetMaxSetsPerPool(10000)
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

void Renderer::InitCullComputeStructures()
{
    m_ComputeCullData.SceneDataUBO.Buffer = Buffer::Builder()
        .SetKind({BufferKind::Uniform})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeFrustumCullData::SceneDataUBO::Data)) * BUFFERED_FRAMES)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();

    Shader* computeCull = Shader::ReflectFrom({"../assets/shaders/compute-cull-comp.shader"});

    ShaderPipelineTemplate computeCullTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeCull)
        .Build();

    m_Scene.AddShaderTemplate(computeCullTemplate, "compute-cull-template");

    m_ComputeCullData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("compute-cull-template"))
        .Build();
}

void Renderer::InitDepthPyramidComputeStructures()
{
    Shader* computeDepthPyramid = Shader::ReflectFrom({"../assets/shaders/depth-pyramid-comp.shader"});

    m_ComputeDepthPyramidData.PipelineTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeDepthPyramid)
        .Build();

    m_ComputeDepthPyramidData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(&m_ComputeDepthPyramidData.PipelineTemplate)
        .Build();
}

void Renderer::InitReprojectionComputeStructures()
{
    Shader* computeReprojection = Shader::ReflectFrom({"../assets/shaders/depth-reprojection-comp.shader"});

    m_ComputeReprojectionData.PipelineTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeReprojection)
        .Build();

    m_ComputeReprojectionData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(&m_ComputeReprojectionData.PipelineTemplate)
        .Build();

    m_ComputeReprojectionData.ReprojectionUBO.Buffer = Buffer::Builder()
        .SetKind({BufferKind::Uniform})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeReprojectionData::ReprojectionUBO::Data)) * BUFFERED_FRAMES)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();
}

void Renderer::InitDilateComputeStructures()
{
    Shader* computeDilate = Shader::ReflectFrom({"../assets/shaders/depth-dilation-comp.shader"});

    m_ComputeDilateData.PipelineTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeDilate)
        .Build();

    m_ComputeDilateData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(&m_ComputeDilateData.PipelineTemplate)
        .Build();
}

void Renderer::InitCompactComputeStructures()
{
    m_ComputeCompactData.CompactUBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Indirect})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCompactData::CompactUBO::Data)) * BUFFERED_FRAMES)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();
    
    Shader* computeCompact = Shader::ReflectFrom({"../assets/shaders/compute-compact-calls-comp.shader"});

    ShaderPipelineTemplate compactTemplate = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache)
        .SetShaderReflection(computeCompact)
        .Build();

    m_Scene.AddShaderTemplate(compactTemplate, "compute-compact");
    
    m_ComputeCompactData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("compute-compact"))
        .Build();

    m_ComputeCompactData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("compute-compact"))
        .AddBinding("u_command_buffer", m_Scene.GetIndirectBuffer(), m_Scene.GetIndirectBuffer().GetSizeBytes(), 0)
        .AddBinding("u_compacted_command_buffer", m_Scene.GetIndirectCompactBuffer(), m_Scene.GetIndirectCompactBuffer().GetSizeBytes(), 0)
        .AddBinding("u_count_buffer", m_ComputeCompactData.CompactUBO.Buffer, vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCompactData::CompactUBO::Data)), 0)
        .Build();
}

void Renderer::ShutDown()
{
    vkDeviceWaitIdle(Driver::DeviceHandle());

    m_Scene.OnShutdown();
    m_ComputeDepthPyramidData.DepthPyramid.reset();
    
    for (auto& framebuffer : m_Framebuffers)
        Framebuffer::Destroy(framebuffer);
    Swapchain::Destroy(m_Swapchain);
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
    for (auto& framebuffer : m_Framebuffers)
        Framebuffer::Destroy(framebuffer);

    Swapchain::Builder newSwapchainBuilder = Swapchain::Builder()
        .DefaultHints()
        .FromDetails(m_Device.GetSurfaceDetails())
        .SetDevice(m_Device)
        .BufferedFrames(BUFFERED_FRAMES)
        .SetSyncStructures(m_Swapchain.GetFrameSync());
    
    Swapchain::Destroy(m_Swapchain);
    
    m_Swapchain = newSwapchainBuilder.BuildManualLifetime();
    m_Framebuffers = m_Swapchain.GetFramebuffers(m_RenderPass);
    m_ComputeDepthPyramidData.DepthPyramid.reset();

    Input::s_MainViewportSize = m_Swapchain.GetSize();
    m_Camera->SetViewport(m_Swapchain.GetSize().x, m_Swapchain.GetSize().y);
}

void Renderer::LoadScene()
{
    m_Scene.OnInit();
    
    ShaderPipelineTemplate::Builder templateBuilder = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache);

    m_MaterialDataSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_MaterialDataSSBO.Materials.size() * sizeof(MaterialGPU))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
    
    m_ObjectDataSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_ObjectDataSSBO.Objects.size() * sizeof(ObjectData))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();


    Shader* bindlessShaderReflection = Shader::ReflectFrom({
    "../assets/shaders/bindless-textures-test-vert.shader",
    "../assets/shaders/bindless-textures-test-frag.shader"});

    ShaderPipelineTemplate bindlessTemplate = templateBuilder
        .SetShaderReflection(bindlessShaderReflection)
        .Build();

    m_Scene.AddShaderTemplate(bindlessTemplate, "bindless");

    m_BindlessData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("bindless"))
        .CompatibleWithVertex(VertexP3N3UV2::GetInputDescriptionDI())
        .SetRenderPass(m_RenderPass)
        .Build();

    m_BindlessData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("bindless"))
        .AddBinding("u_textures", BINDLESS_TEXTURES_COUNT)
        .AddBinding("u_camera_buffer", m_CameraDataUBO.Buffer, sizeof(CameraData), 0)
        .AddBinding("u_scene_data", m_SceneDataUBO.Buffer, sizeof(SceneData), 0)
        .AddBinding("u_object_buffer", m_ObjectDataSSBO.Buffer)
        .AddBinding("u_material_buffer", m_MaterialDataSSBO.Buffer)
        .Build();
    

    Model* car = Model::LoadFromAsset("../assets/models/car/scene.model");
    Model* mori = Model::LoadFromAsset("../assets/models/mori/mori.model");
    Model* gun = Model::LoadFromAsset("../assets/models/gun/scene.model");
    Model* helmet = Model::LoadFromAsset("../assets/models/flight_helmet/FlightHelmet.model");
    Model* tree = Model::LoadFromAsset("../assets/models/tree/scene.model");
    Model* sphere = Model::LoadFromAsset("../assets/models/sphere/scene.model");
   //Model sponza = Model::LoadFromAsset("../assets/models/sponza/scene.model");
   // sponza.Upload(*this);
    
    m_Scene.AddModel(car, "car");
    m_Scene.AddModel(mori, "mori");
    m_Scene.AddModel(gun, "gun");
    m_Scene.AddModel(helmet, "helmet");
    m_Scene.AddModel(tree, "tree");
    m_Scene.AddModel(sphere, "sphere");

    //m_Scene.AddModel(sponza, "sponza");

    std::vector models = {"car", "helmet", "mori", "gun"};

    for (i32 x = -5; x <= 5; x++)
    {
        for (i32 y = -3; y <= 3; y++)
        {
            for (i32 z = -5; z <= 5; z++)
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
