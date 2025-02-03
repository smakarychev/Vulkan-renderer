#pragma once
#include "RenderCommands.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/ResourceHandle.h"

/* with command list it is possible to store commands for later compilation,
 * e.g. for testing purposes
 * but at the moment they're compiled immediately
 */

class RenderCommandList
{
public:
    void SetCommandBuffer(CommandBuffer cmd);
    
    void ExecuteSecondaryCommandBuffer(ExecuteSecondaryBufferCommand&& command);
    
    void PrepareSwapchainPresent(PrepareSwapchainPresentCommand&& command);

    void BeginRendering(BeginRenderingCommand&& command);
    void EndRendering(EndRenderingCommand&& command);
    void BeginImGuiRendering(ImGuiBeginCommand&& command);
    void EndImGuiRendering(ImGuiEndCommand&& command);
    void BeginConditionalRendering(BeginConditionalRenderingCommand&& command);
    void EndConditionalRendering(EndConditionalRenderingCommand&& command);

    void SetViewport(SetViewportCommand&& command);
    void SetScissors(SetScissorsCommand&& command);
    void SetDepthBias(SetDepthBiasCommand&& command);

    void CopyBuffer(CopyBufferCommand&& command);
    void CopyBufferToImage(CopyBufferToImageCommand&& command);

    void CopyImage(CopyImageCommand&& command);
    void BlitImage(BlitImageCommand&& command);

    void WaitOnFullPipelineBarrier(WaitOnFullPipelineBarrierCommand&& command);
    void WaitOnBarrier(WaitOnBarrierCommand&& command);
    void SignalSplitBarrier(SignalSplitBarrierCommand&& command);
    void WaitOnSplitBarrier(WaitOnSplitBarrierCommand&& command);
    void ResetSplitBarrier(ResetSplitBarrierCommand&& command);

    void BindVertexBuffers(BindVertexBuffersCommand&& command);
    void BindIndexU32Buffer(BindIndexU32BufferCommand&& command);
    void BindIndexU16Buffer(BindIndexU16BufferCommand&& command);
    void BindIndexU8Buffer(BindIndexU8BufferCommand&& command);

    void BindPipelineGraphics(BindPipelineGraphicsCommand&& command);
    void BindPipelineCompute(BindPipelineComputeCommand&& command);
    void BindImmutableSamplersGraphics(BindImmutableSamplersGraphicsCommand&& command);
    void BindImmutableSamplersCompute(BindImmutableSamplersComputeCommand&& command);
    void BindDescriptorSetGraphics(BindDescriptorSetGraphicsCommand&& command);
    void BindDescriptorSetCompute(BindDescriptorSetComputeCommand&& command);
    void BindDescriptorsGraphics(BindDescriptorsGraphicsCommand&& command);
    void BindDescriptorsCompute(BindDescriptorsComputeCommand&& command);
    void BindDescriptorArenaAllocators(BindDescriptorArenaAllocatorsCommand&& command);

    void PushConstants(PushConstantsCommand&& command);

    void Draw(DrawCommand&& command);
    void DrawIndexed(DrawIndexedCommand&& command);
    void DrawIndexedIndirect(DrawIndexedIndirectCommand&& command);
    void DrawIndexedIndirectCount(DrawIndexedIndirectCountCommand&& command);

    void Dispatch(DispatchCommand&& command);
    void DispatchIndirect(DispatchIndirectCommand&& command);
private:
    CommandBuffer m_Cmd{};
};
