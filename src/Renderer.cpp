#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "RenderObject.h"
#include "Scene.h"
#include "GLFW/glfw3.h"
#include "Vulkan/RenderCommand.h"
#include "Vulkan/VulkanUtils.h"

Renderer::Renderer()
{
    Init();
    LoadScene();
    UpdateCamera();
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

    SortScene(m_Scene);
    Submit(m_Scene);
    
    EndFrame();
}

void Renderer::OnUpdate()
{
    UpdateCamera();
    UpdateScene();
}

void Renderer::UpdateCamera()
{
    f32 angle = (f32)glfwGetTime();
    glm::vec3 defaultPos = {0.0f, 0.1f, 1.0f};
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angle) * 5, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 pos = glm::vec3(model * glm::vec4(defaultPos, 1.0f));
    glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (f32)m_Swapchain.GetSize().x / (f32)m_Swapchain.GetSize().y, 1e-1f, 1e+3f);
    projection[1][1] *= -1.0f;
    GetFrameContext().CameraDataUBO.CameraData = {.View = view, .Projection = projection, .ViewProjection = projection * view};
    GetFrameContext().CameraDataUBO.Buffer.SetData(&GetFrameContext().CameraDataUBO.CameraData, sizeof(CameraData));
}

void Renderer::UpdateScene()
{
    f32 freq = (f32)glfwGetTime() / 10.0f;
    f32 red = (sin(freq) + 1.0f) * 0.5f;
    f32 green = (cos(freq) + 1.0f) * 0.5f;
    f32 blue = (red + green) * 0.5f;
    f32 sunFreq = (f32)glfwGetTime();
    f32 sunPos = sin(sunFreq);
    m_SceneDataUBO.SceneData.SunlightDirection = {sunPos * 2.0f,(sunPos + 1.0f) * 10.0f, sunPos * 8.0f, 1.0f};
    m_SceneDataUBO.SceneData.AmbientColor = { red, green, blue, 1.0f};
    u64 offsetBytes = vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber;
    m_SceneDataUBO.Buffer.SetData(&m_SceneDataUBO.SceneData, sizeof(SceneData), offsetBytes);

    // assuming that object transform can change
    for (u32 i = 0; i < m_Scene.GetRenderObjects().size(); i++)
        GetFrameContext().ObjectDataSSBO.Objects[i].Transform = m_Scene.GetRenderObjects()[i].Transform;
    
    GetFrameContext().ObjectDataSSBO.Buffer.SetData(GetFrameContext().ObjectDataSSBO.Objects.data(),
        GetFrameContext().ObjectDataSSBO.Objects.size() * sizeof(ObjectData));
}

void Renderer::BeginFrame()
{
    u32 frameNumber = GetFrameContext().FrameNumber;
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    m_SwapchainImageIndex = m_Swapchain.AcquireImage(frameNumber);

    cmd.Reset();
    cmd.Begin();
    
    VkClearValue colorClear = {.color = {{0.1f, 0.1f, 0.1f, 1.0f}}};
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
    cmd.Submit(m_Device.GetQueues().Graphics, sync);
    
    m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, m_SwapchainImageIndex, frameNumber);
    m_FrameNumber++;
    m_CurrentFrameContext = &m_FrameContexts[m_FrameNumber % BUFFERED_FRAMES];
}

void Renderer::Submit(const Scene& scene)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    Mesh* boundMesh = nullptr;
    Material* boundMaterial = nullptr;
    PushConstantBuffer meshPushConstants = {};
    for (u32 i = 0; i < scene.GetRenderObjects().size(); i++)
    {
        auto& object = scene.GetRenderObjects()[i];
        
        meshPushConstants.GetData().Transform = object.Transform;

        if (object.Material != boundMaterial)
        {
            object.Material->Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
            boundMaterial = object.Material;
            u32 uniformOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
            GetFrameContext().GlobalDescriptorSet.Bind(cmd, object.Material->Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, {uniformOffset});
            GetFrameContext().ObjectDescriptorSet.Bind(cmd, object.Material->Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
        }

        //PushConstants(boundMaterial->Pipeline, &meshPushConstants.GetData(), meshPushConstants.GetDescription());

        if (object.Mesh != boundMesh)
        {
            object.Mesh->GetBuffer().Bind(cmd);
            boundMesh = object.Mesh;
        }

        RenderCommand::Draw(cmd, object.Mesh->GetVertexCount(), i);
    }
}

void Renderer::SortScene(Scene& scene)
{
    // sort by material and mesh
    if (!scene.IsDirty())
        return;
    scene.ClearDirty();

    std::sort(scene.GetRenderObjects().begin(), scene.GetRenderObjects().end(),
        [](const RenderObject& a, const RenderObject& b) { return a.Material < b.Material || a.Material == b.Material && a.Mesh < b.Mesh; });
}

void Renderer::Submit(const Mesh& mesh)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    
    mesh.GetBuffer().Bind(cmd);
    RenderCommand::Draw(cmd, mesh.GetVertexCount());
}

void Renderer::PushConstants(const Pipeline& pipeline,const void* pushConstants, const PushConstantDescription& description)
{
    CommandBuffer& cmd = GetFrameContext().CommandBuffer;
    RenderCommand::PushConstants(cmd, pipeline, pushConstants, description);
}

void Renderer::UploadMesh(const Mesh& mesh)
{
    Buffer stageBuffer = Buffer::Builder().
        SetKind(BufferKind::Source).
        SetSizeBytes(mesh.GetBuffer().GetSizeBytes()).
        SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY).
        BuildManualLifetime();

    stageBuffer.SetData(mesh.GetVertices().data(), mesh.GetVertices().size() * sizeof(Vertex3D));

    Renderer::ImmediateUpload(m_Device.GetQueues().Graphics, m_UploadContext, [&](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBuffer(cmd, stageBuffer, mesh.GetBuffer());
    });
    
    Buffer::Destroy(stageBuffer);
}

void Renderer::Init()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(1200, 720, "My window", nullptr, nullptr);

    m_Device = Device::Builder().
        Defaults().
        SetWindow(m_Window).
        Build();

    Driver::Init(m_Device);

    m_UploadContext.CommandPool = CommandPool::Builder().
        SetQueue(QueueKind::Graphics).
        Build();
    m_UploadContext.CommandBuffer = m_UploadContext.CommandPool.AllocateBuffer(CommandBufferKind::Primary);
    m_UploadContext.Fence = Fence::Builder().Build();
    
    m_Swapchain = Swapchain::Builder().
        DefaultHints().
        FromDetails(m_Device.GetSurfaceDetails()).
        SetDevice(m_Device).
        BufferedFrames(BUFFERED_FRAMES).
        Build();

    std::vector<AttachmentTemplate> attachmentTemplates = m_Swapchain.GetAttachmentTemplates();
    
    Subpass subpass = Subpass::Builder().
        SetAttachments(attachmentTemplates).
        Build();

    m_RenderPass = RenderPass::Builder().
        AddSubpass(subpass).
        AddSubpassDependency(
            VK_SUBPASS_EXTERNAL,
            subpass,
            {
                .SourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .SourceAccessMask = 0,
                .DestinationAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            }).
        AddSubpassDependency(
            VK_SUBPASS_EXTERNAL,
            subpass,
            {
                .SourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .SourceAccessMask = 0,
                .DestinationAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
            }).
        Build();

    m_Framebuffers = m_Swapchain.GetFramebuffers(m_RenderPass);

    m_FrameContexts.resize(BUFFERED_FRAMES);
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        CommandPool pool = CommandPool::Builder().
            SetQueue(QueueKind::Graphics).
            PerBufferReset(true).
            Build();
        CommandBuffer buffer = pool.AllocateBuffer(CommandBufferKind::Primary);

        m_FrameContexts[i].CommandPool = pool;
        m_FrameContexts[i].CommandBuffer = buffer;
        m_FrameContexts[i].FrameSync = m_Swapchain.GetFrameSync(i);
        m_FrameContexts[i].FrameNumber = i;
    }

    // descriptors
    m_DescriptorPool = DescriptorPool::Builder().
        Defaults().
        Build();

    m_GlobalDescriptorSetLayout = DescriptorSetLayout::Builder().
        AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).
        AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT).
        Build();

    m_ObjectDescriptorSetLayout = DescriptorSetLayout::Builder().
        AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).
        Build();

    m_SceneDataUBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Uniform).
            SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * BUFFERED_FRAMES).
            SetMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU).
            Build();
        
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        FrameContext& context = m_FrameContexts[i];
        context.CameraDataUBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Uniform).
            SetSizeBytes(sizeof(CameraData)).
            SetMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU).
            Build();

        context.GlobalDescriptorSet = m_DescriptorPool.Allocate(m_GlobalDescriptorSetLayout);
        context.GlobalDescriptorSet.BindBuffer(0, context.CameraDataUBO.Buffer, sizeof(CameraData));
        context.GlobalDescriptorSet.BindBuffer(1, m_SceneDataUBO.Buffer, sizeof(SceneData), 0);

        context.ObjectDataSSBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Storage).
            SetSizeBytes(context.ObjectDataSSBO.Objects.size() * sizeof(ObjectData)).
            SetMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU).
            Build();

        context.ObjectDescriptorSet = m_DescriptorPool.Allocate(m_ObjectDescriptorSetLayout);
        context.ObjectDescriptorSet.BindBuffer(0, context.ObjectDataSSBO.Buffer, context.ObjectDataSSBO.Buffer.GetSizeBytes());
    }
    
    m_CurrentFrameContext = &m_FrameContexts.front();
}

void Renderer::ShutDown()
{
    Driver::Shutdown();
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::LoadScene()
{    
    Material defaultMaterial;
    defaultMaterial.Pipeline = Pipeline::Builder().
        SetRenderPass(m_RenderPass).
        AddShader(ShaderKind::Vertex, "assets/shaders/triangle_big.vert").
        AddShader(ShaderKind::Pixel, "assets/shaders/triangle_big.frag").
        FixedFunctionDefaults().
        SetVertexDescription(Vertex3D::GetInputDescription()).
        AddPushConstant(PushConstantBuffer().GetDescription()).
        AddDescriptorLayout(m_GlobalDescriptorSetLayout).
        AddDescriptorLayout(m_ObjectDescriptorSetLayout).
        Build();

    Material greyMaterial;
    greyMaterial.Pipeline = Pipeline::Builder().
        SetRenderPass(m_RenderPass).
        AddShader(ShaderKind::Vertex, "assets/shaders/grey.vert").
        AddShader(ShaderKind::Pixel, "assets/shaders/grey.frag").
        FixedFunctionDefaults().
        SetVertexDescription(Vertex3D::GetInputDescription()).
        AddPushConstant(PushConstantBuffer().GetDescription()).
        AddDescriptorLayout(m_GlobalDescriptorSetLayout).
        AddDescriptorLayout(m_ObjectDescriptorSetLayout).
        Build();

    Mesh bugatti = Mesh::LoadFromFile("assets/models/bugatti/bugatti.obj");
    Mesh mori = Mesh::LoadFromFile("assets/models/mori/mori.obj");
    Mesh viking_room = Mesh::LoadFromFile("assets/models/viking_room/viking_room.obj");
    UploadMesh(bugatti);
    UploadMesh(mori);
    UploadMesh(viking_room);
    
    m_Scene.AddMaterial(defaultMaterial, "default");
    m_Scene.AddMaterial(greyMaterial, "grey");
    m_Scene.AddMesh(bugatti, "bugatti");
    m_Scene.AddMesh(mori, "mori");
    m_Scene.AddMesh(viking_room, "viking_room");

    std::vector materials = {"default", "grey"};
    std::vector meshes = {"bugatti", "mori", "viking_room"};

    for (i32 x = -10; x <= 10; x++)
    {
        for (i32 z = -10; z <= 10; z++)
        {
            u32 meshIndex = rand() % meshes.size();
            u32 materialIndex = rand() % materials.size();
            
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3((f32)x / 10, 0.0f, (f32)z / 10)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.02f));
            RenderObject newRenderObject;
            newRenderObject.Transform = transform;
            newRenderObject.Mesh = m_Scene.GetMesh(meshes[meshIndex]);
            newRenderObject.Material = m_Scene.GetMaterial(materials[materialIndex]);
            m_Scene.AddRenderObject(newRenderObject);
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
