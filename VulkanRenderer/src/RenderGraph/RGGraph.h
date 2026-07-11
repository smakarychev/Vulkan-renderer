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

    BufferResource Create(StringId name, const RGBufferDescription& description);
    ImageResource Create(StringId name, const RGImageDescription& description);
    ImageResource Create(StringId name, ResourceCreationFlags creationFlags, const RGImageDescription& description);
    PersistentBufferResource AddPersistent(Buffer buffer);
    PersistentImageResource AddPersistent(Image image, ImageLayout layout = ImageLayout::Undefined);
    void UpdatePersistent(PersistentBufferResource resource, Buffer buffer);
    void UpdatePersistent(PersistentImageResource resource, Image image);
    void UpdatePersistent(PersistentImageResource resource, Image image, ImageLayout layout);
    BufferResource ImportPersistent(StringId name, PersistentBufferResource buffer);
    ImageResource ImportPersistent(StringId name, PersistentImageResource image);
    BufferResource Import(StringId name, Buffer buffer);
    ImageResource Import(StringId name, Image image, ImageLayout layout = ImageLayout::Undefined);
    void Export(BufferResource resource, PersistentBufferResource& buffer, DeletionQueue& deletionQueue,
        BufferUsage additionalUsage = BufferUsage::None);
    void Export(ImageResource resource, PersistentImageResource& image, DeletionQueue& deletionQueue,
        ImageUsage additionalUsage = ImageUsage::None);
    ImageResource SplitImage(ImageResource main, ImageSubresourceDescription subresource);
    ImageResource MergeImage(Span<const ImageResource> splits);
    BufferResource ReadBuffer(BufferResource resource, ResourceAccessFlags accessFlags);
    BufferResource WriteBuffer(BufferResource resource, ResourceAccessFlags accessFlags);
    BufferResource ReadWriteBuffer(BufferResource resource, ResourceAccessFlags accessFlags);
    ImageResource ReadImage(ImageResource resource, ResourceAccessFlags accessFlags);
    ImageResource WriteImage(ImageResource resource, ResourceAccessFlags accessFlags);
    ImageResource ReadWriteImage(ImageResource resource, ResourceAccessFlags accessFlags);
    ImageResource RenderTarget(ImageResource resource, const RenderTargetAccessDescription& description);
    ImageResource DepthStencilTarget(ImageResource resource, const DepthStencilTargetAccessDescription& description,
        std::optional<DepthBias> depthBias = std::nullopt);
    void HasSideEffect() const;

    template <typename T>
    BufferResource Upload(BufferResource buffer, T&& data, u64 bufferOffset = 0);

    ImageResource SetBackbufferImage(Image backbuffer, ImageLayout layout = ImageLayout::Undefined);
    ImageResource GetBackbufferImage() const;

    const BufferDescription& GetBufferDescription(BufferResource buffer) const;
    const ImageDescription& GetImageDescription(ImageResource image) const;
    Buffer GetBuffer(BufferResource buffer) const;
    Image GetImage(ImageResource image) const;
    Buffer GetBuffer(PersistentBufferResource buffer) const;
    Image GetImage(PersistentImageResource image) const;
    std::pair<Buffer, const BufferDescription&> GetBufferWithDescription(BufferResource buffer) const;
    std::pair<Image, const ImageDescription&> GetImageWithDescription(ImageResource image) const;
    std::pair<Buffer, const BufferDescription&> GetBufferWithDescription(PersistentBufferResource buffer) const;
    std::pair<Image, const ImageDescription&> GetImageWithDescription(PersistentImageResource image) const;
    bool IsBufferAllocated(BufferResource buffer) const;
    bool IsImageAllocated(ImageResource image) const;

    BufferBinding GetBufferBinding(BufferResource buffer) const;
    ImageBinding GetImageBinding(ImageResource image) const;

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

    const lux::ShaderAsset& SetShader(StringId name) const;
    const lux::ShaderAsset& SetShader(StringId name, StringId variant) const;
    const lux::ShaderAsset& SetShader(StringId name, ShaderOverridesView&& overrides) const;
    const lux::ShaderAsset& SetShader(StringId name, std::optional<StringId> variant,
        ShaderOverridesView&& overrides) const;
    const lux::ShaderAsset& HandleShaderError(StringId name) const;
    const lux::ShaderAsset& GetShader() const;

private:
    ::BufferDescription CreateBufferDescription(const RGBufferDescription& description) const;
    ::ImageDescription CreateImageDescription(const RGImageDescription& description) const;
    ImageSubresourceDescription GetImageSubresourceDescription(ImageResource image) const;
    ImageLayout& GetImageLayout(ImageResource image);

    /* Build a list of dependent passes. Each pass from `list[pass]` is dependent on the execution of `pass` */
    std::vector<std::vector<u32>> BuildDependencyList() const;
    void TopologicalSort(std::vector<std::vector<u32>>& dependencyList);
    void DepthRetopology(const std::vector<u32>& depths, std::vector<u32>& topologicalOrder) const;
    void ProcessVirtualResources();
    using ValidateAccessResult = std::expected<void, std::string>;
    ValidateAccessResult ValidateAccessCommon(const ResourceAccessInfo& info);
    ValidateAccessResult ValidateAccess(const BufferResourceAccess& access, const RGBuffer& buffer);
    ValidateAccessResult ValidateAccess(const ImageResourceAccess& access, const RGImage& image);
    void ValidateImportedResources();
    std::vector<BufferResourceAccessConflict> FindBufferResourceConflicts();
    std::vector<ImageResourceAccessConflict> FindImageResourceConflicts();
    bool HasChangedAllSplitLayouts(std::vector<ImageResourceAccessConflict>& conflicts,
        ImageResourceAccessConflict& conflict, RGImage& image, ImageLayout newLayout);
    void ChangeMainImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
        ImageResourceAccessConflict& conflict, RGImage& image, ImageLayout newLayout);
    void ChangeSubresourceImageLayout(std::vector<ImageResourceAccessConflict>& conflicts,
        ImageResourceAccessConflict& conflict, RGImage& image, u32 subresourceIndex,
        ImageLayout newLayout);
    void ManageBarriers(const std::vector<BufferResourceAccessConflict>& bufferConflicts,
        const std::vector<ImageResourceAccessConflict>& imageConflicts);

    void PreProcessPersistentResources();
    void PostProcessPersistentResources();
    void ResetPersistentResources();

    void SubmitPassUploads(FrameContext& frameContext);

    BufferResource AddBufferAccess(BufferResource resource, AccessType type, RGBuffer& buffer, PipelineStage stage,
        PipelineAccess access);
    ImageResource AddImageAccess(ImageResource resource, AccessType type, RGImage& image, PipelineStage stage,
        PipelineAccess access);
    u32 CurrentPassIndex() const;
    Pass& CurrentPass() const;
    bool IsPassSplitOrMerge(const Pass& pass) const;
    
private:
    ImageResource m_Backbuffer{};
    std::vector<RGBuffer> m_Buffers;
    std::vector<RGImage> m_Images;
    std::vector<BufferResourceAccess> m_BufferAccesses;
    std::vector<ImageResourceAccess> m_ImageAccesses;

    struct PersistentBufferInfo
    {
        Buffer Buffer{};
        BufferResource Resource{};
        DeletionQueue* DeletionQueue{nullptr};
    };

    struct PersistentImageInfo
    {
        Image Image{};
        ImageLayout Layout{ImageLayout::Undefined};
        ImageResource Resource{};
        DeletionQueue* DeletionQueue{nullptr};
    };

    std::vector<PersistentBufferInfo> m_PersistentBuffers;
    std::vector<PersistentImageInfo> m_PersistentImages;

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
    
    const StringId m_SplitPassName{"Split"_hsv};
    const StringId m_MergePassName{"Merge"_hsv};
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
BufferResource Graph::Upload(BufferResource buffer, T&& data, u64 bufferOffset)
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
