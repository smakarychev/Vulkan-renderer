#include "rendererpch.h"

#include "RenderCommandList.h"

#include "Vulkan/Device.h"


void RenderCommandList::SetCommandBuffer(CommandBuffer cmd)
{
    m_Cmd = cmd;
}

void RenderCommandList::ExecuteSecondaryCommandBuffer(ExecuteSecondaryBufferCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::PrepareSwapchainPresent(PrepareSwapchainPresentCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BeginRendering(BeginRenderingCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::EndRendering(EndRenderingCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BeginImGuiRendering(ImGuiBeginCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::EndImGuiRendering(ImGuiEndCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BeginConditionalRendering(BeginConditionalRenderingCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::EndConditionalRendering(EndConditionalRenderingCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::SetViewport(SetViewportCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::SetScissors(SetScissorsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::SetDepthBias(SetDepthBiasCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::CopyBuffer(CopyBufferCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::CopyBufferToImage(CopyBufferToImageCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::CopyImage(CopyImageCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BlitImage(BlitImageCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::WaitOnFullPipelineBarrier(WaitOnFullPipelineBarrierCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::WaitOnBarrier(WaitOnBarrierCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::SignalSplitBarrier(SignalSplitBarrierCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::WaitOnSplitBarrier(WaitOnSplitBarrierCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::ResetSplitBarrier(ResetSplitBarrierCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindVertexBuffers(BindVertexBuffersCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindIndexU32Buffer(BindIndexU32BufferCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindIndexU16Buffer(BindIndexU16BufferCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindIndexU8Buffer(BindIndexU8BufferCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindPipelineGraphics(BindPipelineGraphicsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindPipelineCompute(BindPipelineComputeCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindImmutableSamplersGraphics(BindImmutableSamplersGraphicsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindImmutableSamplersCompute(BindImmutableSamplersComputeCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindDescriptorsGraphics(BindDescriptorsGraphicsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindDescriptorsCompute(BindDescriptorsComputeCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::BindDescriptorArenaAllocators(BindDescriptorArenaAllocatorsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::PushConstants(PushConstantsCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::Draw(DrawCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::DrawIndexed(DrawIndexedCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::DrawIndexedIndirect(DrawIndexedIndirectCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::DrawIndexedIndirectCount(DrawIndexedIndirectCountCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::Dispatch(DispatchCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}

void RenderCommandList::DispatchIndirect(DispatchIndirectCommand&& command)
{
    Device::CompileCommand(m_Cmd, command);
}