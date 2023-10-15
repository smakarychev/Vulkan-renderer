#include "Renderer.h"

#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "RenderObject.h"
#include "Scene.h"
#include "Model.h"
#include "Core/Camera.h"
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
}

void Renderer::UpdateCameraBuffers()
{
    m_CameraDataUBO.CameraData = {.View = m_Camera->GetView(), .Projection = m_Camera->GetProjection(), .ViewProjection = m_Camera->GetViewProjection()};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber;
    m_ResourceUploader.UpdateBuffer(m_CameraDataUBO.Buffer, &m_CameraDataUBO.CameraData, sizeof(CameraData), offsetBytes);
}

void Renderer::UpdateScene()
{
    if (m_Scene.IsDirty())
    {
        m_Scene.UpdateRenderObject(m_BindlessData.DescriptorSet, m_BindlessData.BindlessDescriptorsState);
        SortScene(m_Scene);
        m_Scene.CreateIndirectBatches();
        m_Scene.ClearDirty();

        u32 mappedBuffer = m_ResourceUploader.GetMappedBuffer(m_DrawIndirectBuffer.GetSizeBytes());
        VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)m_ResourceUploader.GetMappedAddress(mappedBuffer);

        for (u32 i = 0; i < m_Scene.GetRenderObjects().size(); i++)
        {
            const auto& object = m_Scene.GetRenderObjects()[i];
            commands[i].firstIndex = 0;
            commands[i].indexCount = object.Mesh->GetIndexCount();
            commands[i].firstInstance = i;
            commands[i].instanceCount = 1;
            commands[i].vertexOffset = 0;
        }

        m_ResourceUploader.UpdateBuffer(m_DrawIndirectBuffer, mappedBuffer, 0);
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
        m_MaterialDataSSBO.Materials[i].Albedo = m_Scene.GetRenderObjects()[i].MaterialBindless->Albedo;
        m_MaterialDataSSBO.Materials[i].AlbedoTextureIndex = m_Scene.GetRenderObjects()[i].MaterialBindless->AlbedoTextureIndex;
    }

    m_ResourceUploader.UpdateBuffer(m_ObjectDataSSBO.Buffer, m_ObjectDataSSBO.Objects.data(), m_ObjectDataSSBO.Objects.size() * sizeof(ObjectData), 0);
    m_ResourceUploader.UpdateBuffer(m_MaterialDataSSBO.Buffer, m_MaterialDataSSBO.Materials.data(), m_MaterialDataSSBO.Materials.size() * sizeof(MaterialBindless), 0);
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
    
    VkClearValue colorClear = {.color = {{0.05f, 0.05f, 0.05f, 1.0f}}};
    VkClearValue depthClear = {.depthStencil = {.depth = 1.0f}};
    m_RenderPass.Begin(cmd, m_Framebuffers[m_SwapchainImageIndex], {colorClear, depthClear});

    RenderCommand::SetViewport(cmd, m_Swapchain.GetSize());
    RenderCommand::SetScissors(cmd, {0, 0}, m_Swapchain.GetSize());

    // todo: is this the best place for it?
    m_ResourceUploader.SubmitUpload();
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

void Renderer::Submit(const Scene& scene)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;

    ShaderPipeline lastPipeline;

    m_BindlessData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    const PipelineLayout& layout = m_BindlessData.Pipeline.GetPipelineLayout();
    
    u32 cameraDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(CameraData)) * GetFrameContext().FrameNumber);
    u32 sceneDataOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Global, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, {cameraDataOffset, sceneDataOffset});
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Material, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
    
    for (auto& batch : scene.GetIndirectBatches())
    {
        batch.Mesh->GetVertexBuffer().Bind(cmd);
        batch.Mesh->GetIndexBuffer().Bind(cmd);

        u32 stride = sizeof(VkDrawIndexedIndirectCommand);
        u64 bufferOffset = (u64)batch.First * stride;
        RenderCommand::DrawIndexedIndirect(cmd, m_DrawIndirectBuffer, bufferOffset, batch.Count, stride);
    }

    m_BindlessData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.Bind(cmd, DescriptorKind::Pass, m_BindlessData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    m_BindlessData.DescriptorSet.SetTexture("u_textures", *m_Scene.GetTexture("texture_../assets/models/gun/scene.model6"), 0);
    m_BindlessData.DescriptorSet.SetTexture("u_textures", *m_Scene.GetTexture("texture_../assets/models/gun/scene.model6"), 1);
}

void Renderer::SortScene(Scene& scene)
{
    std::sort(scene.GetRenderObjects().begin(), scene.GetRenderObjects().end(),
        [](const RenderObject& a, const RenderObject& b) { return a.Mesh < b.Mesh; });
}

void Renderer::Submit(const Mesh& mesh)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    mesh.GetVertexBuffer().Bind(cmd);
    RenderCommand::Draw(cmd, mesh.GetVertexCount());
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

    // indirect commands preparation
    m_DrawIndirectBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::ShutDown()
{
    vkDeviceWaitIdle(Driver::DeviceHandle());
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
    Shader defaultShaderReflection = {};
    defaultShaderReflection.ReflectFrom({"../assets/shaders/triangle_big-vert.shader", "../assets/shaders/triangle_big-frag.shader"});

    Shader greyShaderReflection = {};
    greyShaderReflection.ReflectFrom({"../assets/shaders/grey-vert.shader", "../assets/shaders/grey-frag.shader"});

    Shader texturedShaderReflection = {};
    texturedShaderReflection.ReflectFrom({"../assets/shaders/textured-vert.shader", "../assets/shaders/textured-frag.shader"});

    ShaderPipelineTemplate::Builder templateBuilder = ShaderPipelineTemplate::Builder()
        .SetDescriptorAllocator(&m_PersistentDescriptorAllocator)
        .SetDescriptorLayoutCache(&m_LayoutCache);
    
    ShaderPipelineTemplate defaultTemplate = templateBuilder
        .SetShaderReflection(&defaultShaderReflection)
        .Build();

    ShaderPipelineTemplate greyTemplate = templateBuilder
        .SetShaderReflection(&greyShaderReflection)
        .Build();

    ShaderPipelineTemplate texturedTemplate = templateBuilder
        .SetShaderReflection(&texturedShaderReflection)
        .Build();

    m_Scene.AddShaderTemplate(defaultTemplate, "default");
    m_Scene.AddShaderTemplate(greyTemplate, "grey");
    m_Scene.AddShaderTemplate(texturedTemplate, "textured");

    m_MaterialDataSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_MaterialDataSSBO.Materials.size() * sizeof(MaterialBindless))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
    
    m_ObjectDataSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_ObjectDataSSBO.Objects.size() * sizeof(ObjectData))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_GlobalObjectSet = ShaderDescriptorSet::Builder()
       .SetTemplate(m_Scene.GetShaderTemplate("default"))
       .AddBinding("u_camera_buffer", m_CameraDataUBO.Buffer, sizeof(CameraData), 0)
       .AddBinding("u_scene_data", m_SceneDataUBO.Buffer, sizeof(SceneData), 0)
       .AddBinding("u_object_buffer", m_ObjectDataSSBO.Buffer)
       .Build();

    Model car = Model::LoadFromAsset("../assets/models/car/scene.model");
    Model mori = Model::LoadFromAsset("../assets/models/mori/mori.model");
    Model gun = Model::LoadFromAsset("../assets/models/gun/scene.model");
    Model helmet = Model::LoadFromAsset("../assets/models/flight_helmet/FlightHelmet.model");
    Model tree = Model::LoadFromAsset("../assets/models/tree/scene.model");
   //Model sponza = Model::LoadFromAsset("../assets/models/sponza/scene.model");
    car.Upload(*this);
    mori.Upload(*this);
    gun.Upload(*this);
    helmet.Upload(*this);
    tree.Upload(*this);
   // sponza.Upload(*this);
    
    m_Scene.AddModel(car, "car");
    m_Scene.AddModel(mori, "mori");
    m_Scene.AddModel(gun, "gun");
    m_Scene.AddModel(helmet, "helmet");
    m_Scene.AddModel(tree, "tree");
    //m_Scene.AddModel(sponza, "sponza");

    std::vector models = {"helmet", "car", "gun", "tree"};

    for (i32 x = -5; x <= 5; x++)
    {
        for (i32 z = -5; z <= 5; z++)
        {
            u32 modelIndex = rand() % models.size();
            
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(x * 3.0f, 0.0f, z * 3.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

            RenderObject newRenderObject;
            newRenderObject.Transform = transform;

            Model* model = m_Scene.GetModel(models[modelIndex]);
            model->CreateRenderObjects(&m_Scene, transform);
        }
    }

    Shader bindlessShaderReflection = {};
    bindlessShaderReflection.ReflectFrom({"../assets/shaders/bindless-textures-test-vert.shader", "../assets/shaders/bindless-textures-test-frag.shader"});

    ShaderPipelineTemplate bindlessTemplate = templateBuilder
        .SetShaderReflection(&bindlessShaderReflection)
        .Build();

    m_Scene.AddShaderTemplate(bindlessTemplate, "bindless");

    m_BindlessData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_Scene.GetShaderTemplate("bindless"))
        .CompatibleWithVertex(VertexP3N3UV::GetInputDescription())
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
}

const FrameContext& Renderer::GetFrameContext() const
{
    return *m_CurrentFrameContext;
}

FrameContext& Renderer::GetFrameContext()
{
    return *m_CurrentFrameContext;
}
