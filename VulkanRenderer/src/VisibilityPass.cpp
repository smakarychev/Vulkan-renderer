#include "VisibilityPass.h"

#include "Mesh.h"
#include "utils/SceneUtils.h"
#include "Scene.h"
#include "SceneCull.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    // todo: maybe is in not the best format?
    static constexpr VkFormat VISIBILITY_BUFFER_FORMAT = VK_FORMAT_R32G32_UINT;
}

VisibilityBuffer::VisibilityBuffer(const glm::uvec2& size, const CommandBuffer& cmd)
{
    m_VisibilityImage = Image::Builder()
        .SetExtent({size.x, size.y})
        .SetFormat(VISIBILITY_BUFFER_FORMAT) // todo: maybe is in not the best format?
        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .BuildManualLifetime();

    PipelineImageBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .DependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .Image = &m_VisibilityImage,
        .ImageSourceMask = VK_ACCESS_SHADER_WRITE_BIT,
        .ImageDestinationMask = VK_ACCESS_SHADER_READ_BIT,
        .ImageSourceLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .ImageDestinationLayout = VK_IMAGE_LAYOUT_GENERAL,
        .ImageAspect = VK_IMAGE_ASPECT_COLOR_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

VisibilityBuffer::~VisibilityBuffer()
{
    Image::Destroy(m_VisibilityImage);
}

void VisibilityPass::Init(const VisibilityPassInitInfo& initInfo)
{
    if (m_VisibilityBuffer)
    {
        m_VisibilityBuffer.reset();
        ShaderDescriptorSet::Destroy(m_DescriptorSet);
    }
    else
    {
        m_Template = sceneUtils::loadShaderPipelineTemplate(
            {"../assets/shaders/processed/visibility-buffer/visibility-buffer-vert.shader" , "../assets/shaders/processed/visibility-buffer/visibility-buffer-frag.shader"}, "visibility-pipeline",
            *initInfo.Scene, *initInfo.DescriptorAllocator, *initInfo.LayoutCache);

        RenderingDetails details = initInfo.RenderingDetails;
        details.ColorFormats = {VISIBILITY_BUFFER_FORMAT};
        
        m_Pipeline = ShaderPipeline::Builder()
            .SetTemplate(m_Template)
            .SetRenderingDetails(details)
            .CompatibleWithVertex(VertexP3N3UV2::GetInputDescriptionDI())
            .Build();
    }
    
    m_VisibilityBuffer = std::make_unique<VisibilityBuffer>(initInfo.Size, *initInfo.Cmd);
}

void VisibilityPass::ShutDown()
{
    if (m_VisibilityBuffer)
        m_VisibilityBuffer.reset();
}
