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
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angle) * 5.0f, glm::vec3(0.0f, 1.0f, 0.0f));
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

    if (m_Scene.IsDirty())
    {
        SortScene(m_Scene);
        m_Scene.CreateIndirectBatches();
        m_Scene.ClearDirty();

        for (u32 i = 0; i < BUFFERED_FRAMES; i++)
            m_FrameContexts[i].IsDrawIndirectBufferDirty = true;
    }

    if (GetFrameContext().IsDrawIndirectBufferDirty)
    {
        Buffer stageBuffer = Buffer::Builder().
            SetKind(BufferKind::Source).
            SetSizeBytes(GetFrameContext().DrawIndirectBuffer.GetSizeBytes()).
            SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
            BuildManualLifetime();
        
        VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)stageBuffer.Map();

        for (u32 i = 0; i < m_Scene.GetRenderObjects().size(); i++)
        {
            const auto& object = m_Scene.GetRenderObjects()[i];
            commands[i].firstIndex = 0;
            commands[i].indexCount = object.Mesh->GetIndexCount();
            commands[i].firstInstance = i;
            commands[i].instanceCount = 1;
            commands[i].vertexOffset = 0;
        }

        stageBuffer.Unmap();
        
        ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, GetFrameContext().DrawIndirectBuffer);
        });

        Buffer::Destroy(stageBuffer);
        
        GetFrameContext().IsDrawIndirectBufferDirty = false;
    }
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
    
    for (auto& batch : scene.GetIndirectBatches())
    {
        batch.Material->Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
        u32 uniformOffset = u32(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * GetFrameContext().FrameNumber);
        GetFrameContext().GlobalObjectSet.Bind(cmd, DescriptorKind::Global, batch.Material->Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, {uniformOffset});
        GetFrameContext().GlobalObjectSet.Bind(cmd, DescriptorKind::Pass, batch.Material->Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
        if (batch.Material->TextureSet.has_value())
            batch.Material->TextureSet->Bind(cmd, DescriptorKind::Material, batch.Material->Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
        batch.Mesh->GetVertexBuffer().Bind(cmd);
        batch.Mesh->GetIndexBuffer().Bind(cmd);

        u32 stride = sizeof(VkDrawIndexedIndirectCommand);
        u64 bufferOffset = (u64)batch.First * stride;
        RenderCommand::DrawIndexedIndirect(cmd, GetFrameContext().DrawIndirectBuffer, bufferOffset, batch.Count, stride);
    }
}

void Renderer::SortScene(Scene& scene)
{
    std::sort(scene.GetRenderObjects().begin(), scene.GetRenderObjects().end(),
        [](const RenderObject& a, const RenderObject& b) { return a.Material < b.Material || a.Material == b.Material && a.Mesh < b.Mesh; });
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

void Renderer::Init()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(1600, 900, "My window", nullptr, nullptr);

    m_Device = Device::Builder().
        Defaults().
        SetWindow(m_Window).
        Build();

    Driver::Init(m_Device);
    
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
    m_PersistentDescriptorAllocator = DescriptorAllocator::Builder().
        SetMaxSetsPerPool(1000).
        Build();

    m_SceneDataUBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Uniform).
            SetSizeBytes(vkUtils::alignUniformBufferSizeBytes(sizeof(SceneData)) * BUFFERED_FRAMES).
            SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
            Build();

    // indirect commands preparation
    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        FrameContext& context = m_FrameContexts[i];
        context.DrawIndirectBuffer = Buffer::Builder().
            SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination}).
            SetSizeBytes(sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAW_INDIRECT_CALLS).
            SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT).
            Build();
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
    ShaderReflection defaultShaderReflection = {};
    defaultShaderReflection.ReflectFrom({"../assets/shaders/triangle_big-vert.spv", "../assets/shaders/triangle_big-frag.spv"});

    ShaderReflection greyShaderReflection = {};
    greyShaderReflection.ReflectFrom({"../assets/shaders/grey-vert.spv", "../assets/shaders/grey-frag.spv"});

    ShaderReflection texturedShaderReflection = {};
    texturedShaderReflection.ReflectFrom({"../assets/shaders/textured-vert.spv", "../assets/shaders/textured-frag.spv"});

    ShaderPipelineTemplate::Builder templateBuilder = ShaderPipelineTemplate::Builder().
        SetDescriptorAllocator(&m_PersistentDescriptorAllocator).
        SetDescriptorLayoutCache(&m_LayoutCache);
    
    ShaderPipelineTemplate defaultTemplate = templateBuilder.
        SetShaderReflection(&defaultShaderReflection).
        Build();

    ShaderPipelineTemplate greyTemplate = templateBuilder.
        SetShaderReflection(&greyShaderReflection).
        Build();

    ShaderPipelineTemplate texturedTemplate = templateBuilder.
        SetShaderReflection(&texturedShaderReflection).
        Build();

    m_Scene.AddShaderTemplate(defaultTemplate, "default");
    m_Scene.AddShaderTemplate(greyTemplate, "grey");
    m_Scene.AddShaderTemplate(texturedTemplate, "textured");
    
    Material defaultMaterial;
    defaultMaterial.Pipeline = ShaderPipeline::Builder().
        SetTemplate(m_Scene.GetShaderTemplate("default")).
        CompatibleWithVertex(Vertex3D::GetInputDescription()).
        SetRenderPass(m_RenderPass).
        Build();

    Material greyMaterial;
    greyMaterial.Pipeline = ShaderPipeline::Builder().
        SetTemplate(m_Scene.GetShaderTemplate("grey")).
        CompatibleWithVertex(Vertex3D::GetInputDescription()).
        SetRenderPass(m_RenderPass).
        Build();

    Material textured;
    Image texture = Image::Builder().
        FormAssetFile("../assets/textures/texture.tx").
        SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT).
        Build();
    m_Scene.AddTexture(texture, "texture");
    textured.TextureSet = ShaderDescriptorSet::Builder().
        SetTemplate(m_Scene.GetShaderTemplate("textured")).
        AddBinding("u_texture", *m_Scene.GetTexture("texture")).
        Build();
    textured.Pipeline = ShaderPipeline::Builder().
        SetTemplate(m_Scene.GetShaderTemplate("textured")).
        CompatibleWithVertex(Vertex3D::GetInputDescription()).
        SetRenderPass(m_RenderPass).
        Build();

    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        FrameContext& context = m_FrameContexts[i];

        context.CameraDataUBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Uniform).
            SetSizeBytes(sizeof(CameraData)).
            SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
            Build();

        context.ObjectDataSSBO.Buffer = Buffer::Builder().
            SetKind(BufferKind::Storage).
            SetSizeBytes(context.ObjectDataSSBO.Objects.size() * sizeof(ObjectData)).
            SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
            Build();
        
        context.GlobalObjectSet = ShaderDescriptorSet::Builder().
            SetTemplate(m_Scene.GetShaderTemplate("default")).
            AddBinding("u_camera_buffer", context.CameraDataUBO.Buffer, sizeof(CameraData), 0).
            AddBinding("dyn_u_scene_data", m_SceneDataUBO.Buffer, sizeof(SceneData), 0).
            AddBinding("u_object_buffer", context.ObjectDataSSBO.Buffer).
            Build();
    }

    Mesh bugatti = Mesh::LoadFromAsset("../assets/models/bugatti/bugatti.msh");
    Mesh mori = Mesh::LoadFromAsset("../assets/models/mori/mori.msh");
    Mesh viking_room = Mesh::LoadFromAsset("../assets/models/viking_room/viking_room.msh");
    bugatti.Upload(*this);
    mori.Upload(*this);
    viking_room.Upload(*this);
    
    m_Scene.AddMaterial(defaultMaterial, "default");
    m_Scene.AddMaterial(greyMaterial, "grey");
    m_Scene.AddMaterial(textured, "textured");
    m_Scene.AddMesh(bugatti, "bugatti");
    m_Scene.AddMesh(mori, "mori");
    m_Scene.AddMesh(viking_room, "viking_room");

    std::vector materials = {"default", "grey", "textured"};
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
