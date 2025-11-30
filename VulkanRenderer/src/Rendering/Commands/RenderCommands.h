#pragma once

#include "RenderHandle.h"
#include "RenderGraph/Passes/Generated/Types/IndirectCommandUniform.generated.h"
#include "Rendering/Buffer/Buffer.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/Image/Image.h"
#include "Rendering/Pipeline.h"
#include "Rendering/RenderingInfo.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Synchronization.h"

enum class RenderCommandType
{
    None = 0,

    /* meta */
    ExecuteSecondaryBuffer,
    
    /* swapchain */
    SwapchainPreparePresent,

    /* rendering */
    BeginRendering,
    EndRendering,
    
    ImGuiBegin,
    ImGuiEnd,
    
    BeginConditionalRendering,
    EndConditionalRendering,

    SetViewport,
    SetScissors,
    SetDepthBias,
    
    /* buffer commands */
    BufferCopy,
    BufferCopyToImage,

    /* image commands */
    ImageCopy,
    ImageBlit,

    /* synchronization */
    BarrierFull,
    Barrier,
    SignalSplitBarrier,
    WaitSplitBarrier,
    ResetSplitBarrier,

    /* binds */
    BindVertexBuffers,
    BindIndexU32Buffer,
    BindIndexU16Buffer,
    BindIndexU8Buffer,

    BindPipeline,
    BindImmutableSamplers,
    BindDescriptors,
    BindDescriptorArenaAllocators,

    PushConstants,
    
    /* draws */
    Draw,
    DrawIndexed,
    DrawIndexedIndirect,
    DrawIndexedIndirectCount,

    /* dispatches */
    Dispatch,
    DispatchIndirect,
};

enum class RenderCommandQueueType
{
    None = 0,
    Graphics = BIT(1),
    Compute  = BIT(2),
    Transfer = BIT(3),

    All = Graphics | Compute | Transfer,
    GC = Graphics | Compute,
};
CREATE_ENUM_FLAGS_OPERATORS(RenderCommandQueueType)

struct RenderCommand
{
    RenderCommandType Type{RenderCommandType::None};
    RenderCommandQueueType QueueType{RenderCommandQueueType::None};
    RenderCommand(RenderCommandType type, RenderCommandQueueType queueType) : Type(type), QueueType(queueType) {}
};

template <RenderCommandType CType, RenderCommandQueueType QType>
struct RenderCommandTyped : RenderCommand
{
    static constexpr RenderCommandType TYPE = CType;
    static constexpr RenderCommandQueueType QUEUE_TYPE = QType;
    RenderCommandTyped() : RenderCommand(TYPE, QUEUE_TYPE) { }
};


struct ExecuteSecondaryBufferCommand
    : RenderCommandTyped<RenderCommandType::ExecuteSecondaryBuffer, RenderCommandQueueType::All>
{
    CommandBuffer Cmd{};
};

struct PrepareSwapchainPresentCommand
    : RenderCommandTyped<RenderCommandType::SwapchainPreparePresent, RenderCommandQueueType::Transfer>
{
    Swapchain Swapchain{};
    u32 ImageIndex{};
};

struct BeginRenderingCommand
    : RenderCommandTyped<RenderCommandType::BeginRendering, RenderCommandQueueType::Graphics>
{
    RenderingInfo RenderingInfo{};
};
struct EndRenderingCommand
    : RenderCommandTyped<RenderCommandType::EndRendering, RenderCommandQueueType::Graphics>
{
};

struct ImGuiBeginCommand : RenderCommandTyped<RenderCommandType::ImGuiBegin, RenderCommandQueueType::Graphics>
{
};
struct ImGuiEndCommand : RenderCommandTyped<RenderCommandType::ImGuiEnd, RenderCommandQueueType::Graphics>
{
    RenderingInfo RenderingInfo{};
};

struct BeginConditionalRenderingCommand
    : RenderCommandTyped<RenderCommandType::BeginConditionalRendering, RenderCommandQueueType::Graphics>
{
    Buffer Buffer{};
    u64 Offset{};
};
struct EndConditionalRenderingCommand
    : RenderCommandTyped<RenderCommandType::EndConditionalRendering, RenderCommandQueueType::Graphics>
{
};

struct SetViewportCommand : RenderCommandTyped<RenderCommandType::SetViewport, RenderCommandQueueType::Graphics>
{
    glm::vec2 Size{};
};
struct SetScissorsCommand : RenderCommandTyped<RenderCommandType::SetScissors, RenderCommandQueueType::Graphics>
{
    glm::vec2 Offset{};
    glm::vec2 Size{};
};
struct SetDepthBiasCommand : RenderCommandTyped<RenderCommandType::SetDepthBias, RenderCommandQueueType::Graphics>
{
    f32 Constant{0.0f};
    f32 Slope{0.0f};
};

struct CopyBufferCommand : RenderCommandTyped<RenderCommandType::BufferCopy, RenderCommandQueueType::Transfer>
{
    Buffer Source{};
    Buffer Destination{};
    u64 SizeBytes{};
    u64 SourceOffset{};
    u64 DestinationOffset{};
};
struct CopyBufferToImageCommand
    : RenderCommandTyped<RenderCommandType::BufferCopyToImage, RenderCommandQueueType::Transfer>
{
    Buffer Buffer{};
    Image Image{};
    u64 SizeBytes{};
    u64 BufferOffset{};
    ImageSubresourceDescription ImageSubresource{};
};

struct ImageSubregion
{
    u32 Mipmap{0};
    u32 LayerBase{0};
    u32 Layers{(u32)ImageSubresourceDescription::ALL_LAYERS};
    glm::uvec3 Bottom{};
    glm::uvec3 Top{};
};

struct CopyImageCommand : RenderCommandTyped<RenderCommandType::ImageCopy, RenderCommandQueueType::Transfer>
{
    Image Source{};
    Image Destination{};
    ImageSubregion SourceSubregion{};
    ImageSubregion DestinationSubregion{};
};
struct BlitImageCommand : RenderCommandTyped<RenderCommandType::ImageBlit, RenderCommandQueueType::Transfer>
{
    Image Source{};
    Image Destination{};
    ImageFilter Filter{ImageFilter::Linear};
    ImageSubregion SourceSubregion{};
    ImageSubregion DestinationSubregion{};
};


struct WaitOnFullPipelineBarrierCommand
    : RenderCommandTyped<RenderCommandType::BarrierFull, RenderCommandQueueType::All>
{
};
struct WaitOnBarrierCommand
    : RenderCommandTyped<RenderCommandType::Barrier, RenderCommandQueueType::All>
{
    DependencyInfo DependencyInfo{};
};
struct SignalSplitBarrierCommand
    : RenderCommandTyped<RenderCommandType::SignalSplitBarrier, RenderCommandQueueType::All>
{
    SplitBarrier SplitBarrier{};
    DependencyInfo DependencyInfo{};
};
struct WaitOnSplitBarrierCommand
    : RenderCommandTyped<RenderCommandType::WaitSplitBarrier, RenderCommandQueueType::All>
{
    SplitBarrier SplitBarrier{};
    DependencyInfo DependencyInfo{};
};
struct ResetSplitBarrierCommand
    : RenderCommandTyped<RenderCommandType::ResetSplitBarrier, RenderCommandQueueType::All>
{
    SplitBarrier SplitBarrier{};
    DependencyInfo DependencyInfo{};
};

struct BindVertexBuffersCommand
    : RenderCommandTyped<RenderCommandType::BindVertexBuffers, RenderCommandQueueType::GC>
{
    Span<const Buffer> Buffers{};
    Span<const u64> Offsets{};
};
struct BindIndexU32BufferCommand
    : RenderCommandTyped<RenderCommandType::BindIndexU32Buffer, RenderCommandQueueType::GC>
{
    Buffer Buffer{};
    u64 Offset{0};
};
struct BindIndexU16BufferCommand
    : RenderCommandTyped<RenderCommandType::BindIndexU16Buffer, RenderCommandQueueType::GC>
{
    Buffer Buffer{};
    u64 Offset{0};
};
struct BindIndexU8BufferCommand
    : RenderCommandTyped<RenderCommandType::BindIndexU8Buffer, RenderCommandQueueType::GC>
{
    Buffer Buffer{};
    u64 Offset{0};
};

struct BindPipelineGraphicsCommand
    : RenderCommandTyped<RenderCommandType::BindPipeline, RenderCommandQueueType::Graphics>
{
    Pipeline Pipeline{};
};
struct BindPipelineComputeCommand
    : RenderCommandTyped<RenderCommandType::BindPipeline, RenderCommandQueueType::Compute>
{
    Pipeline Pipeline{};
};
struct BindImmutableSamplersGraphicsCommand
    : RenderCommandTyped<RenderCommandType::BindImmutableSamplers, RenderCommandQueueType::Graphics>
{
    Descriptors Descriptors{};
    PipelineLayout PipelineLayout{};
    u32 Set{};
};
struct BindImmutableSamplersComputeCommand
    : RenderCommandTyped<RenderCommandType::BindImmutableSamplers, RenderCommandQueueType::Compute>
{
    Descriptors Descriptors{};
    PipelineLayout PipelineLayout{};
    u32 Set{};
};
struct BindDescriptorsGraphicsCommand
    : RenderCommandTyped<RenderCommandType::BindDescriptors, RenderCommandQueueType::Graphics>
{
    Descriptors Descriptors{};
    const DescriptorArenaAllocators* Allocators{nullptr};
    PipelineLayout PipelineLayout{};
    u32 Set{};
};
struct BindDescriptorsComputeCommand
    : RenderCommandTyped<RenderCommandType::BindDescriptors, RenderCommandQueueType::Compute>
{
    Descriptors Descriptors{};
    const DescriptorArenaAllocators* Allocators{nullptr};
    PipelineLayout PipelineLayout{};
    u32 Set{};
};
struct BindDescriptorArenaAllocatorsCommand
    : RenderCommandTyped<RenderCommandType::BindDescriptorArenaAllocators, RenderCommandQueueType::GC>
{
    const DescriptorArenaAllocators* Allocators{nullptr};
};

struct PushConstantsCommand : RenderCommandTyped<RenderCommandType::PushConstants, RenderCommandQueueType::GC>
{
    PipelineLayout PipelineLayout{};
    Span<const std::byte> Data{};
};

struct DrawCommand : RenderCommandTyped<RenderCommandType::Draw, RenderCommandQueueType::Graphics>
{
    u32 VertexCount{};
    u32 BaseInstance{};
};
struct DrawIndexedCommand : RenderCommandTyped<RenderCommandType::DrawIndexed, RenderCommandQueueType::Graphics>
{
    u32 IndexCount{};
    u32 BaseInstance{};
};
struct IndirectDrawCommand : gen::IndirectCommand {};
struct DrawIndexedIndirectCommand
    : RenderCommandTyped<RenderCommandType::DrawIndexedIndirect, RenderCommandQueueType::Graphics>
{
    Buffer Buffer{};
    u64 Offset{0};
    u32 Count{0};
    u32 Stride{sizeof(IndirectDrawCommand)};
};
struct DrawIndexedIndirectCountCommand
    : RenderCommandTyped<RenderCommandType::DrawIndexedIndirectCount, RenderCommandQueueType::Graphics>
{
    Buffer DrawBuffer{};
    u64 DrawOffset{};
    Buffer CountBuffer{};
    u64 CountOffset{};
    u32 MaxCount{};
    u32 Stride{sizeof(IndirectDrawCommand)};
};

struct DispatchCommand : RenderCommandTyped<RenderCommandType::Dispatch, RenderCommandQueueType::Compute>
{
    glm::uvec3 Invocations{};
    glm::uvec3 GroupSize{1};
};
struct IndirectDispatchCommand
{
    u32 GroupX;
    u32 GroupY;
    u32 GroupZ;
};
struct DispatchIndirectCommand
    : RenderCommandTyped<RenderCommandType::DispatchIndirect, RenderCommandQueueType::Compute>
{
    Buffer Buffer{};
    u64 Offset{};
};
