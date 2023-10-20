#include "Renderer.h"

#include <algorithm>
#include <glm/ext/matrix_transform.hpp>

#include "RenderObject.h"
#include "Scene.h"
#include "Model.h"
#include "Core/Input.h"
#include "Core/Random.h"
#include "GLFW/glfw3.h"
#include "Vulkan/RenderCommand.h"
#include "Vulkan/VulkanUtils.h"

Renderer::Renderer() = default;

void Renderer::Init()
{
    InitRenderingStructures();
    LoadScene();
    InitCullComputeStructures();

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
        CullCompute(m_Scene);

        // todo: remove me, please
        BeginGraphics();
        Submit(m_Scene);
        EndFrame();
    }
    else
    {
        m_FrameEarlyExit = false;
    }
}

void Renderer::OnUpdate()
{
    m_CameraController->OnUpdate(1.0f / 60.0f);
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
    m_ComputeCullData.SceneDataUBO.SceneData.TotalMeshCount = (u32)m_Scene.GetRenderObjects().size(); 
    m_ComputeCullData.SceneDataUBO.SceneData.FrustumPlanes = m_Camera->GetFrustumPlanes(); 
    u64 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCullData::SceneDataUBO::Data)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_ComputeCullData.SceneDataUBO.Buffer, &m_ComputeCullData.SceneDataUBO.SceneData,
        sizeof(ComputeCullData::SceneDataUBO::Data), offset);
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
    VkClearValue depthClear = {.depthStencil = {.depth = 1.0f}};
    m_RenderPass.Begin(cmd, m_Framebuffers[m_SwapchainImageIndex], {colorClear, depthClear});

    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());
}

void Renderer::EndFrame()
{
    u32 frameNumber = GetFrameContext().FrameNumber;
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    SwapchainFrameSync& sync = GetFrameContext().FrameSync;
    
    m_RenderPass.End(cmd);

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

void Renderer::CullCompute(const Scene& scene)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    u32 offset = vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCullData::SceneDataUBO::Data)) * GetFrameContext().FrameNumber;
    
    m_ComputeCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ComputeCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ComputeCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset});
    RenderCommand::Dispatch(cmd, {m_Scene.GetRenderObjects().size() / 64 + 1, 1, 1});
    RenderCommand::CreateBarrier(cmd, {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &m_Device.GetQueues().Graphics,
        .Buffer = &scene.GetIndirectBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
}

void Renderer::Submit(const Scene& scene)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;

    m_BindlessData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_BindlessData.Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber);
    u32 sceneDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset, sceneDataOffset});
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Material, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);

    scene.Bind(cmd);
    RenderCommand::DrawIndexedIndirect(cmd, scene.GetIndirectBuffer(), 0, (u32)scene.GetRenderObjects().size(),  sizeof(VkDrawIndexedIndirectCommand));
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
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCullData::SceneDataUBO::Data)) * BUFFERED_FRAMES)
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

    m_ComputeCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("compute-cull-template"))
        .AddBinding("u_object_buffer", m_Scene.GetRenderObjectsBuffer(), m_Scene.GetRenderObjectsBuffer().GetSizeBytes(), 0)
        .AddBinding("u_command_buffer", m_Scene.GetIndirectBuffer(), m_Scene.GetIndirectBuffer().GetSizeBytes(), 0)
        .AddBinding("u_scene_data", m_ComputeCullData.SceneDataUBO.Buffer, vkUtils::alignUniformBufferSizeBytes(sizeof(ComputeCullData::SceneDataUBO::Data)), 0)
        .Build();
}

void Renderer::ShutDown()
{
    vkDeviceWaitIdle(Driver::DeviceHandle());

    m_Scene.OnShutdown();
    
    for (auto& framebuffer : m_Framebuffers)
        Framebuffer::Destroy(framebuffer);
    Swapchain::Destroy(m_Swapchain);
    m_ResourceUploader.ShutDown();
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

    std::vector models = {"car", "gun", "helmet"};

    for (i32 x = -5; x <= 5; x++)
    {
        for (i32 y = -2; y <= 2; y++)
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
