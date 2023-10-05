#include "RenderPass.h"

#include "Core/core.h"
#include "Driver.h"
#include "RenderCommand.h"

Subpass Subpass::Builder::Build()
{
    return Subpass::Create(m_CreateInfo);
}

Subpass::Builder& Subpass::Builder::AddAttachment(const AttachmentTemplate& attachment)
{
    Driver::Unpack(attachment, m_CreateInfo);

    return *this;
}

Subpass::Builder& Subpass::Builder::SetAttachments(const std::vector<AttachmentTemplate>& attachments)
{
    m_CreateInfo.Attachments = {};
    m_CreateInfo.ColorReferences = {};
    m_CreateInfo.DepthStencilReference = {};
    m_CreateInfo.Attachments.reserve(attachments.size());

    for (auto& attachment : attachments)
        Driver::Unpack(attachment, m_CreateInfo);

    return *this;
}

Subpass Subpass::Create(const Builder::CreateInfo& createInfo)
{
    Subpass subpass = {};

    subpass.m_Attachments = createInfo.Attachments;
    subpass.m_ColorReferences = createInfo.ColorReferences;
    subpass.m_DepthStencilReference = createInfo.DepthStencilReference;

    return subpass;
}

RenderPass RenderPass::Builder::Build()
{
    FinishSubpasses();
    RenderPass renderPass = RenderPass::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([renderPass](){ RenderPass::Destroy(renderPass); });

    return renderPass;
}

RenderPass RenderPass::Builder::BuildManualLifetime()
{
    FinishSubpasses();
    return RenderPass::Create(m_CreateInfo);
}

RenderPass::Builder& RenderPass::Builder::AddSubpass(const Subpass& subpass)
{
    m_Subpasses.push_back(&subpass);

    return *this;
}

RenderPass::Builder& RenderPass::Builder::AddSubpassDependency(u32 source, const Subpass& destination, const SubpassDependencyInfo& dependencyInfo)
{
    auto it = std::ranges::find(m_Subpasses, &destination);
    ASSERT(it != m_Subpasses.end(), "Unknown subpass")
    u32 destinationIndex = (u32)(it - m_Subpasses.begin());

    VkSubpassDependency subpassDependency = {};
    subpassDependency.srcSubpass = source;
    subpassDependency.dstSubpass = destinationIndex;
    subpassDependency.srcStageMask = dependencyInfo.SourceStage;
    subpassDependency.dstStageMask = dependencyInfo.DestinationStage;
    subpassDependency.srcAccessMask = dependencyInfo.SourceAccessMask;
    subpassDependency.dstAccessMask = dependencyInfo.DestinationAccessMask;

    m_CreateInfo.SubpassDependencies.push_back(subpassDependency);

    return *this;
}

void RenderPass::Builder::FinishSubpasses()
{
    for (auto* subpass : m_Subpasses)
        Driver::Unpack(*subpass, m_CreateInfo);
}

RenderPass RenderPass::Create(const Builder::CreateInfo& createInfo)
{
    RenderPass renderPass = {};
    
    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.subpassCount = (u32)createInfo.Subpasses.size();
    renderPassCreateInfo.pSubpasses = createInfo.Subpasses.data();
    renderPassCreateInfo.attachmentCount = (u32)createInfo.Attachments.size();
    renderPassCreateInfo.pAttachments = createInfo.Attachments.data();
    renderPassCreateInfo.dependencyCount = (u32)createInfo.SubpassDependencies.size();
    renderPassCreateInfo.pDependencies = createInfo.SubpassDependencies.data();

    VulkanCheck(vkCreateRenderPass(Driver::DeviceHandle(), &renderPassCreateInfo, nullptr, &renderPass.m_RenderPass),
        "Failed to create render pass");
    
    return renderPass;
}

void RenderPass::Destroy(const RenderPass& renderPass)
{
    vkDestroyRenderPass(Driver::DeviceHandle(), renderPass.m_RenderPass, nullptr);
}

void RenderPass::Begin(const CommandBuffer& commandBuffer, const Framebuffer& framebuffer, const std::vector<VkClearValue>& clearValues)
{
    RenderCommand::BeginRenderPass(commandBuffer, *this, framebuffer, clearValues);
}

void RenderPass::End(const CommandBuffer& commandBuffer)
{
    RenderCommand::EndRenderPass(commandBuffer);
}
