#pragma once

#include "RGGraphPool.h"
#include "RGPass.h"
#include "RGAccess.h"
#include "RGBlackboard.h"
#include "RGResourceUploader.h"

namespace lux
{
class ShaderAssetManager;
}

namespace RG
{
class Pass;
struct GlobalResources;
class GraphWatcher;

class Graph
{
public:
    Graph() = default;
    Graph(const std::array<DescriptorArenaAllocators, BUFFERED_FRAMES>& descriptorAllocators,
        lux::ShaderAssetManager& shaderAssetManager);
    Graph(Graph&) = delete;
    Graph& operator=(Graph&) = delete;
    Graph(Graph&&) = delete;
    Graph& operator=(Graph&&) = delete;
    ~Graph() = default;

    void SetDescriptorAllocators(const std::array<DescriptorArenaAllocators, BUFFERED_FRAMES>& descriptorAllocators);
    void SetWatcher(GraphWatcher& watcher);
    void RemoveWatcher();

    template <typename PassData, typename SetupFn, typename ExecuteFn>
        requires PassCallbacksFn<SetupFn, ExecuteFn, PassData>
    PassData& AddRenderPass(StringId name, SetupFn&& setup, ExecuteFn&& callback);

    void OnFrameBegin(FrameContext& frameContext);
    void Compile(FrameContext& frameContext);
    void Execute(FrameContext& frameContext);
    void Reset();

    Resource Create(StringId name, const RGBufferDescription& description);
    Resource Create(StringId name, const RGImageDescription& description);
    Resource Create(StringId name, ResourceCreationFlags creationFlags, const RGImageDescription& description);
    Resource Import(StringId name, Buffer buffer);
    Resource Import(StringId name, Image image, ImageLayout layout = ImageLayout::Undefined);
    void MarkBufferForExport(Resource resource, BufferUsage additionalUsage = BufferUsage::None);
    void MarkImageForExport(Resource resource, ImageUsage additionalUsage = ImageUsage::None);
    void ClaimBuffer(Resource exported, Buffer& target, DeletionQueue& deletionQueue);
    void ClaimImage(Resource exported, Image& target, DeletionQueue& deletionQueue);
    Resource SplitImage(Resource main, ImageSubresourceDescription subresource);
    Resource MergeImage(Span<const Resource> splits);
    Resource ReadBuffer(Resource resource, ResourceAccessFlags accessFlags);
    Resource WriteBuffer(Resource resource, ResourceAccessFlags accessFlags);
    Resource ReadWriteBuffer(Resource resource, ResourceAccessFlags accessFlags);
    Resource ReadImage(Resource resource, ResourceAccessFlags accessFlags);
    Resource WriteImage(Resource resource, ResourceAccessFlags accessFlags);
    Resource ReadWriteImage(Resource resource, ResourceAccessFlags accessFlags);
    Resource RenderTarget(Resource resource, const RenderTargetAccessDescription& description);
    Resource DepthStencilTarget(Resource resource, const DepthStencilTargetAccessDescription& description,
        std::optional<DepthBias> depthBias = std::nullopt);
    void HasSideEffect() const;

    template <typename T>
    Resource Upload(Resource buffer, T&& data, u64 bufferOffset = 0);

    Resource SetBackbufferImage(Image backbuffer, ImageLayout layout = ImageLayout::Undefined);
    Resource GetBackbufferImage() const;

    const BufferDescription& GetBufferDescription(Resource buffer) const;
    const ImageDescription& GetImageDescription(Resource image) const;
    Buffer GetBuffer(Resource buffer) const;
    Image GetImage(Resource image) const;
    std::pair<Buffer, const BufferDescription&> GetBufferWithDescription(Resource buffer) const;
    std::pair<Image, const ImageDescription&> GetImageWithDescription(Resource image) const;
    bool IsBufferAllocated(Resource buffer) const;
    bool IsImageAllocated(Resource image) const;

    BufferBinding GetBufferBinding(Resource buffer) const;
    ImageBinding GetImageBinding(Resource image) const;

    DescriptorArenaAllocators& GetFrameAllocators() const;
    Blackboard& GetBlackboard();
    const GlobalResources& GetGlobalResources() const;

    template <typename Value>
    void UpdateBlackboard(Value&& value) const;
    template <typename Value>
    Value& GetBlackboardValue() const;
    template <typename Value>
    Value* TryGetBlackboardValue() const;
    template <typename Value>
    Value& GetOrCreateBlackboardValue() const;

    template <typename Value>
    void UpdateBlackboard(Value&& value, u64 hash) const;
    template <typename Value>
    Value& GetBlackboardValue(u64 hash) const;
    template <typename Value>
    Value* TryGetBlackboardValue(u64 hash) const;
    template <typename Value>
    Value& GetOrCreateBlackboardValue(u64 hash) const;

    const lux::Shader& SetShader(StringId name) const;
    const lux::Shader& SetShader(StringId name, StringId variant) const;
    const lux::Shader& SetShader(StringId name, ShaderOverridesView&& overrides) const;
    const lux::Shader& SetShader(StringId name, std::optional<StringId> variant, ShaderOverridesView&& overrides) const;
    const lux::Shader& HandleShaderError(StringId name) const;
    const lux::Shader& GetShader() const;

private:
    ::BufferDescription CreateBufferDescription(const RGBufferDescription& description) const;
    ::ImageDescription CreateImageDescription(const RGImageDescription& description) const;
    ImageSubresourceDescription GetImageSubresourceDescription(Resource image) const;
    ImageLayout& GetImageLayout(Resource image);

    /* Build a list of dependent passes. Each pass from `list[pass]` is dependent on the execution of `pass` */
    std::vector<std::vector<u32>> BuildDependencyList() const;
    void TopologicalSort(std::vector<std::vector<u32>>& dependencyList);
    void DepthRetopology(const std::vector<u32>& depths, std::vector<u32>& topologicalOrder) const;
    void ProcessVirtualResources();
    using ValidateAccessResult = std::expected<void, std::string>;
    ValidateAccessResult ValidateAccess(const ResourceAccess& access, const ResourceBase& base);
    void ValidateImportedResources();
    std::vector<BufferResourceAccessConflict> FindBufferResourceConflicts();
    std::vector<ImageResourceAccessConflict> FindImageResourceConflicts();
    bool HasChangedAllSplitLayouts(std::vector<ImageResourceAccessConflict>& conflicts,
        ResourceAccessConflict& baseConflict, ImageResource& image, ImageLayout newLayout);
    void ChangeMainImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
        ResourceAccessConflict& baseConflict, ImageResource& image, ImageLayout newLayout);
    void ChangeSubresourceImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
        ResourceAccessConflict& baseConflict, ImageResource& image, u32 subresourceIndex, ImageLayout newLayout);
    void ManageBarriers(const std::vector<BufferResourceAccessConflict>& bufferConflicts,
        const std::vector<ImageResourceAccessConflict>& imageConflicts);

    void CheckForUnclaimedExportedResources();

    void SubmitPassUploads(FrameContext& frameContext);

    Resource AddBufferAccess(Resource resource, AccessType type, ResourceBase& resourceBase, PipelineStage stage,
        PipelineAccess access);
    Resource AddImageAccess(Resource resource, AccessType type, ImageResource& image, PipelineStage stage,
        PipelineAccess access);
    u32 CurrentPassIndex() const;
    Pass& CurrentPass() const;

private:
    Resource m_Backbuffer{};
    std::vector<BufferResource> m_Buffers;
    std::vector<ImageResource> m_Images;
    std::vector<ResourceAccess> m_BufferAccesses;
    std::vector<ResourceAccess> m_ImageAccesses;

    std::vector<Resource> m_ExportedBuffers;
    std::vector<Resource> m_ExportedImages;

    std::vector<std::unique_ptr<Pass>> m_Passes;
    std::vector<u32> m_PassIndicesStack{};

    GraphPool m_ResourcesPool;
    RG::ResourceUploader m_ResourceUploader;
    DeletionQueue* m_FrameDeletionQueue{nullptr};

    GraphWatcher* m_GraphWatcher{nullptr};

    std::array<DescriptorArenaAllocators, BUFFERED_FRAMES> m_ArenaAllocators;
    DescriptorArenaAllocators* m_FrameAllocators{&m_ArenaAllocators[0]};
    lux::ShaderAssetManager* m_ShaderAssetManager{nullptr};
    mutable Blackboard m_Blackboard;
};

template <typename PassData, typename SetupFn, typename ExecuteFn>
    requires PassCallbacksFn<SetupFn, ExecuteFn, PassData>
PassData& Graph::AddRenderPass(StringId name, SetupFn&& setup, ExecuteFn&& callback)
{
    m_PassIndicesStack.push_back((u32)m_Passes.size());
    m_Passes.emplace_back(std::make_unique<Pass>(name));
    auto& pass = *m_Passes.back();


    PassData passData = {};
    setup(*this, passData);

    pass.m_ExecutionCallback = std::make_unique<Pass::ExecutionCallback<PassData, ExecuteFn>>(
        passData, std::forward<ExecuteFn>(callback));

    m_PassIndicesStack.pop_back();

    return *(PassData*)pass.m_ExecutionCallback->GetPassData();
}

template <typename T>
Resource Graph::Upload(Resource buffer, T&& data, u64 bufferOffset)
{
    m_ResourceUploader.UpdateBuffer(CurrentPass(), buffer, std::forward<T>(data), bufferOffset);
    m_Buffers[buffer.m_Index].Description.Usage |= BufferUsage::Destination;

    return WriteBuffer(buffer, ResourceAccessFlags::Copy);
}

template <typename Value>
void Graph::UpdateBlackboard(Value&& value) const
{
    m_Blackboard.Update(std::forward<Value>(value));
}

template <typename Value>
Value& Graph::GetBlackboardValue() const
{
    return m_Blackboard.Get<Value>();
}

template <typename Value>
Value* Graph::TryGetBlackboardValue() const
{
    return m_Blackboard.TryGet<Value>();
}

template <typename Value>
Value& Graph::GetOrCreateBlackboardValue() const
{
    if (!TryGetBlackboardValue<Value>())
        UpdateBlackboard(Value{});

    return GetBlackboardValue<Value>();
}

template <typename Value>
void Graph::UpdateBlackboard(Value&& value, u64 hash) const
{
    m_Blackboard.Update(std::forward<Value>(value), hash);
}

template <typename Value>
Value& Graph::GetBlackboardValue(u64 hash) const
{
    return m_Blackboard.Get<Value>(hash);
}

template <typename Value>
Value* Graph::TryGetBlackboardValue(u64 hash) const
{
    return m_Blackboard.TryGet<Value>(hash);
}

template <typename Value>
Value& Graph::GetOrCreateBlackboardValue(u64 hash) const
{
    if (!TryGetBlackboardValue<Value>(hash))
        UpdateBlackboard(Value{}, hash);

    return GetBlackboardValue<Value>(hash);
}
}
