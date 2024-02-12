#include "Synchronization.h"

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanCore.h"

namespace
{
    constexpr VkPipelineStageFlags2 vulkanPipelineStageFromPipelineStage(PipelineStage stage)
    {
        std::vector<std::pair<PipelineStage, VkPipelineStageFlags2>> MAPPINGS {
            {PipelineStage::Top,                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT},
            {PipelineStage::Indirect,               VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT},
            {PipelineStage::VertexInput,            VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT},
            {PipelineStage::IndexInput,             VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT},
            {PipelineStage::AttributeInput,         VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT},
            {PipelineStage::VertexShader,           VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT},
            {PipelineStage::HullShader,             VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT},
            {PipelineStage::DomainShader,           VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT},
            {PipelineStage::GeometryShader,         VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT},
            {PipelineStage::FragmentShader,         VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
            {PipelineStage::DepthEarly,             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT},
            {PipelineStage::DepthLate,              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT},
            {PipelineStage::ColorOutput,            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
            {PipelineStage::ComputeShader,          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
            {PipelineStage::Copy,                   VK_PIPELINE_STAGE_2_COPY_BIT},
            {PipelineStage::Blit,                   VK_PIPELINE_STAGE_2_BLIT_BIT},
            {PipelineStage::Resolve,                VK_PIPELINE_STAGE_2_RESOLVE_BIT},
            {PipelineStage::Clear,                  VK_PIPELINE_STAGE_2_CLEAR_BIT},
            {PipelineStage::AllTransfer,            VK_PIPELINE_STAGE_2_TRANSFER_BIT},
            {PipelineStage::AllGraphics,            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
            {PipelineStage::AllPreRasterization,    VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT},
            {PipelineStage::AllCommands,            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
            {PipelineStage::Bottom,                 VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT},
            {PipelineStage::Host,                   VK_PIPELINE_STAGE_2_HOST_BIT},
            {PipelineStage::TransformFeedback,      VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT},
            {PipelineStage::ConditionalRendering,   VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT},
        };
        
        VkPipelineStageFlags2 flags = 0;
        for (auto&& [ps, vulkanPs] : MAPPINGS)
            if (enumHasAny(stage, ps))
                flags |= vulkanPs;

        return flags;
    }

    constexpr VkAccessFlagBits2 vulkanAccessFlagsFromPipelineAccess(PipelineAccess access)
    {
        std::vector<std::pair<PipelineAccess, VkAccessFlagBits2>> MAPPINGS {
            {PipelineAccess::ReadIndirect,                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
            {PipelineAccess::ReadIndex,                     VK_ACCESS_2_INDEX_READ_BIT},
            {PipelineAccess::ReadAttribute,                 VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT},
            {PipelineAccess::ReadUniform,                   VK_ACCESS_2_UNIFORM_READ_BIT},
            {PipelineAccess::ReadInputAttachment,           VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadColorAttachment,           VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadDepthStencilAttachment,    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT},
            {PipelineAccess::ReadTransfer,                  VK_ACCESS_2_TRANSFER_READ_BIT},
            {PipelineAccess::ReadHost,                      VK_ACCESS_2_HOST_READ_BIT},
            {PipelineAccess::ReadSampled,                   VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
            {PipelineAccess::ReadStorage,                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT},
            {PipelineAccess::ReadShader,                    VK_ACCESS_2_SHADER_READ_BIT},
            {PipelineAccess::ReadAll,                       VK_ACCESS_2_MEMORY_READ_BIT},
            {PipelineAccess::WriteColorAttachment,          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
            {PipelineAccess::WriteDepthStencilAttachment,   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
            {PipelineAccess::WriteTransfer,                 VK_ACCESS_2_TRANSFER_WRITE_BIT},
            {PipelineAccess::WriteHost,                     VK_ACCESS_2_HOST_WRITE_BIT},
            {PipelineAccess::WriteShader,                   VK_ACCESS_2_SHADER_WRITE_BIT},
            {PipelineAccess::WriteAll,                      VK_ACCESS_2_MEMORY_WRITE_BIT},
            {PipelineAccess::ReadFeedbackCounter,           VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT},
            {PipelineAccess::WriteFeedbackCounter,          VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT},
            {PipelineAccess::WriteFeedback,                 VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT},
            {PipelineAccess::ReadConditional,               VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT},
        };
        
        VkAccessFlagBits2 flags = 0;
        for (auto&& [a, vulkanA] : MAPPINGS)
            if (enumHasAny(access, a))
                flags |= vulkanA;

        return flags;
    }

    constexpr VkDependencyFlags vulkanDependencyFlagsFromPipelineDependencyFlags(
        PipelineDependencyFlags dependencyFlags)
    {
        std::vector<std::pair<PipelineDependencyFlags, VkDependencyFlags>> MAPPINGS {
            {PipelineDependencyFlags::ByRegion,     VK_DEPENDENCY_BY_REGION_BIT},
            {PipelineDependencyFlags::DeviceGroup,  VK_DEPENDENCY_DEVICE_GROUP_BIT},
            {PipelineDependencyFlags::FeedbackLoop, VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT},
            {PipelineDependencyFlags::LocalView,    VK_DEPENDENCY_VIEW_LOCAL_BIT},
        };

        VkDependencyFlags flags = 0;
        for (auto&& [d, vulkanD] : MAPPINGS)
            if (enumHasAny(dependencyFlags, d))
                flags |= vulkanD;

        return flags;
    }
}

Fence Fence::Builder::Build()
{
    Fence fence = Fence::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([fence](){ Fence::Destroy(fence); });

    return fence;
}

Fence Fence::Builder::BuildManualLifetime()
{
    return Fence::Create(m_CreateInfo);
}

Fence::Builder& Fence::Builder::StartSignaled(bool isSignaled)
{
    if (isSignaled)
        m_CreateInfo.Flags |= VK_FENCE_CREATE_SIGNALED_BIT;
    else
        m_CreateInfo.Flags &= ~VK_FENCE_CREATE_SIGNALED_BIT;

    return *this;
}

Fence Fence::Create(const Builder::CreateInfo& createInfo)
{
    Fence fence = {};
    
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = createInfo.Flags;

    VulkanCheck(vkCreateFence(Driver::DeviceHandle(), &fenceCreateInfo, nullptr, &fence.m_Fence),
        "Failed to create fence");

    return fence;
}

void Fence::Destroy(const Fence& semaphore)
{
    vkDestroyFence(Driver::DeviceHandle(), semaphore.m_Fence, nullptr);
}

void Fence::Reset() const
{
    RenderCommand::ResetFence(*this);
}

void Fence::Wait() const
{
    RenderCommand::WaitForFence(*this);
}

bool Fence::Check() const
{
    return RenderCommand::CheckFence(*this) == VK_SUCCESS;
}

Semaphore Semaphore::Builder::Build()
{
    Semaphore semaphore = Semaphore::Create({});
    Driver::DeletionQueue().AddDeleter([semaphore](){ Semaphore::Destroy(semaphore); });

    return semaphore;
}

Semaphore Semaphore::Builder::BuildManualLifetime()
{
    return Semaphore::Create({});
}

Semaphore Semaphore::Create(const Builder::CreateInfo& createInfo)
{
    Semaphore semaphore = {};

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VulkanCheck(vkCreateSemaphore(Driver::DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphore.m_Semaphore),
        "Failed to create semaphore");
    Driver::DeviceHandle();
    
    return semaphore;
}

void Semaphore::Destroy(const Semaphore& semaphore)
{
    vkDestroySemaphore(Driver::DeviceHandle(), semaphore.m_Semaphore, nullptr);
}

TimelineSemaphore TimelineSemaphore::Builder::Build()
{
    TimelineSemaphore semaphore = TimelineSemaphore::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([semaphore](){ TimelineSemaphore::Destroy(semaphore); });

    return semaphore;
}

TimelineSemaphore TimelineSemaphore::Builder::BuildManualLifetime()
{
    return TimelineSemaphore::Create(m_CreateInfo);
}

TimelineSemaphore::Builder& TimelineSemaphore::Builder::InitialValue(u64 value)
{
    m_CreateInfo.InitialValue = value;

    return *this;
}

TimelineSemaphore TimelineSemaphore::Create(const Builder::CreateInfo& createInfo)
{
    TimelineSemaphore semaphore = {};

    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = createInfo.InitialValue;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    vkCreateSemaphore(Driver::DeviceHandle(), &semaphoreCreateInfo, nullptr, &semaphore.m_Semaphore);

    return semaphore;
}

void TimelineSemaphore::Destroy(const TimelineSemaphore& semaphore)
{
    vkDestroySemaphore(Driver::DeviceHandle(), semaphore.m_Semaphore, nullptr);
}

void TimelineSemaphore::WaitCPU(u64 value)
{
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_Semaphore;
    waitInfo.pValues = &value;
    
    VulkanCheck(vkWaitSemaphores(Driver::DeviceHandle(), &waitInfo, UINT64_MAX),
        "Failed to wait for timeline semaphore");
}

void TimelineSemaphore::SignalCPU(u64 value)
{
    VkSemaphoreSignalInfo signalInfo = {};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_Semaphore;
    signalInfo.value = value;

    VulkanCheck(vkSignalSemaphore(Driver::DeviceHandle(), &signalInfo),
        "Failed to signal semaphore");

    m_Timeline = value;
}

DependencyInfo DependencyInfo::Builder::Build()
{
    return DependencyInfo::Create(m_CreateInfo);
}

DependencyInfo::Builder& DependencyInfo::Builder::SetFlags(PipelineDependencyFlags flags)
{
    m_CreateInfo.DependencyFlags = vulkanDependencyFlagsFromPipelineDependencyFlags(flags);

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::ExecutionDependency(
    const ExecutionDependencyInfo& executionDependencyInfo)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(executionDependencyInfo.SourceStage);
    memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(executionDependencyInfo.DestinationStage);
    m_CreateInfo.ExecutionDependencyInfo = memoryBarrier;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::MemoryDependency(const MemoryDependencyInfo& memoryDependencyInfo)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(memoryDependencyInfo.SourceStage);
    memoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(memoryDependencyInfo.DestinationStage);
    memoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(memoryDependencyInfo.SourceAccess);
    memoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(memoryDependencyInfo.DestinationAccess);
    m_CreateInfo.MemoryDependencyInfo = memoryBarrier;

    return *this;
}

DependencyInfo::Builder& DependencyInfo::Builder::LayoutTransition(const LayoutTransitionInfo& layoutTransitionInfo)
{
    VkImageMemoryBarrier2 imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageMemoryBarrier.srcStageMask = vulkanPipelineStageFromPipelineStage(layoutTransitionInfo.SourceStage);
    imageMemoryBarrier.dstStageMask = vulkanPipelineStageFromPipelineStage(layoutTransitionInfo.DestinationStage);
    imageMemoryBarrier.srcAccessMask = vulkanAccessFlagsFromPipelineAccess(layoutTransitionInfo.SourceAccess);
    imageMemoryBarrier.dstAccessMask = vulkanAccessFlagsFromPipelineAccess(layoutTransitionInfo.DestinationAccess);
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    m_CreateInfo.LayoutTransitionInfo = imageMemoryBarrier;
    
    Driver::Unpack(layoutTransitionInfo, m_CreateInfo);

    return *this;
}

DependencyInfo DependencyInfo::Create(const Builder::CreateInfo& createInfo)
{
    DependencyInfo dependencyInfo = {};

    if (createInfo.ExecutionDependencyInfo.has_value())
        dependencyInfo.m_ExecutionMemoryDependenciesInfo.push_back(*createInfo.ExecutionDependencyInfo);
    if (createInfo.MemoryDependencyInfo.has_value())
        dependencyInfo.m_ExecutionMemoryDependenciesInfo.push_back(*createInfo.MemoryDependencyInfo);
    if (createInfo.LayoutTransitionInfo.has_value())
        dependencyInfo.m_LayoutTransitionInfo.push_back(*createInfo.LayoutTransitionInfo);

    dependencyInfo.m_DependencyInfo = {};
    dependencyInfo.m_DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.m_DependencyInfo.dependencyFlags = createInfo.DependencyFlags;

    return dependencyInfo;
}


void Barrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnBarrier(cmd, dependencyInfo);
}

SplitBarrier SplitBarrier::Builder::Build()
{
    SplitBarrier splitBarrier = SplitBarrier::Create({});
    Driver::DeletionQueue().AddDeleter([splitBarrier](){ SplitBarrier::Destroy(splitBarrier); });

    return splitBarrier;
}

SplitBarrier SplitBarrier::Builder::BuildManualLifetime()
{
    return SplitBarrier::Create({});
}

SplitBarrier SplitBarrier::Create(const Builder::CreateInfo& createInfo)
{
    SplitBarrier splitBarrier = {};
    
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    VulkanCheck(vkCreateEvent(Driver::DeviceHandle(), &eventCreateInfo, nullptr, &splitBarrier.m_Event),
        "Failed to create split barrier");

    return splitBarrier;
}

void SplitBarrier::Destroy(const SplitBarrier& splitBarrier)
{
    vkDestroyEvent(Driver::DeviceHandle(), splitBarrier.m_Event, nullptr);
}

void SplitBarrier::Signal(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::SignalSplitBarrier(cmd, *this, dependencyInfo);
}

void SplitBarrier::Wait(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::WaitOnSplitBarrier(cmd, *this, dependencyInfo);
}

void SplitBarrier::Reset(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo) const
{
    RenderCommand::ResetSplitBarrier(cmd, *this, dependencyInfo);
}
