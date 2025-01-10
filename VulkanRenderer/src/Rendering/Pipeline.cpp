#include "Pipeline.h"

#include "Vulkan/RenderCommand.h"

void Pipeline::BindGraphics(const CommandBuffer& commandBuffer) const
{
    RenderCommand::BindGraphics(commandBuffer, *this);
}

void Pipeline::BindCompute(const CommandBuffer& commandBuffer) const
{
    RenderCommand::BindCompute(commandBuffer, *this);
}
