#include "Renderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "GLFW/glfw3.h"
#include "Vulkan/RenderCommand.h"

Renderer::Renderer()
{
    Init();
    LoadModels();
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
    u32 imageIndex = m_Swapchain.AcquireImage();

    m_CommandBuffer.Begin();

    f32 green = (std::sinf((f32)glfwGetTime()) + 1.0f) * 0.5f;
    VkClearValue colorClear = {.color = {{0.2f, green, 0.3f, 1.0f}}};
    VkClearValue depthClear = {.depthStencil = {.depth = 1.0f}};
    m_RenderPass.Begin(m_CommandBuffer, m_Framebuffers[imageIndex], {colorClear, depthClear});

    RenderCommand::SetViewport(m_CommandBuffer, m_Swapchain.GetSize());
    RenderCommand::SetScissors(m_CommandBuffer, {0, 0}, m_Swapchain.GetSize());

    m_Pipeline.Bind(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    PushConstants(&m_MeshPushConstants.GetData(), m_MeshPushConstants.GetDescription());
    Submit(*m_Mesh);
    
    m_RenderPass.End(m_CommandBuffer);

    m_CommandBuffer.End();
    m_CommandBuffer.Submit(m_Device.GetQueues().Graphics, m_Swapchain.GetFrameSync());
    
    m_Swapchain.PresentImage(m_Device.GetQueues().Presentation, imageIndex);
}

void Renderer::OnUpdate()
{
    f32 angle = (f32)glfwGetTime();
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), 10.0f * glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.1f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (f32)m_Swapchain.GetSize().x / (f32)m_Swapchain.GetSize().y, 1e-1f, 1e+3f);
    projection[1][1] *= -1.0f;
    m_MeshPushConstants.GetData().Transform = projection * view * model;
}

void Renderer::Submit(const Mesh& mesh)
{
    RenderCommand::Draw(m_CommandBuffer, mesh.GetVertexCount(), mesh.GetBuffer());
}

void Renderer::PushConstants(const void* pushConstants, const PushConstantDescription& description)
{
    RenderCommand::PushConstants(m_CommandBuffer, m_Pipeline, pushConstants, description);
}

void Renderer::Init()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    m_Window = glfwCreateWindow(600, 400, "My window", nullptr, nullptr);

    m_Device = Device::Builder().
        Defaults().
        SetWindow(m_Window).
        Build();

    Driver::Init(m_Device);
    
    m_Swapchain = Swapchain::Builder().
        DefaultHints().
        FromDetails(m_Device.GetSurfaceDetails()).
        SetDevice(m_Device).
        BufferedFrames(1).
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
        SetDevice(m_Device).
        Build();

    m_Framebuffers = m_Swapchain.GetFramebuffers(m_RenderPass);
    
    m_Pipeline = Pipeline::Builder().
        SetRenderPass(m_RenderPass).
        AddShader(ShaderKind::Vertex, "assets/shaders/triangle_big.vert").
        AddShader(ShaderKind::Pixel, "assets/shaders/triangle_big.frag").
        FixedFunctionDefaults().
        SetVertexDescription(Vertex3D::GetInputDescription()).
        AddPushConstant(m_MeshPushConstants.GetDescription()).
        Build();

    m_CommandPool = CommandPool::Builder().
        SetQueue(m_Device, QueueKind::Graphics).
        PerBufferReset(true).
        Build();

    m_CommandBuffer = m_CommandPool.AllocateBuffer(CommandBufferKind::Primary);
}

void Renderer::ShutDown()
{
    Driver::Shutdown(m_Device);
    glfwDestroyWindow(m_Window); // optional (glfwTerminate does same thing)
    glfwTerminate();
}

void Renderer::LoadModels()
{
    m_Mesh = std::make_unique<Mesh>(Mesh::LoadFromFile("assets/models/bugatti/bugatti.obj"));
}
