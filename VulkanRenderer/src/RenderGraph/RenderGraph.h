#pragma once

#include <memory>
#include <unordered_set>
#include <vector>

#include "RGBlackboard.h"
#include "RGResource.h"
#include "RenderPass.h"
#include "RGCommon.h"
#include "RGResourceUploader.h"
#include "Vulkan/Device.h"

struct CVarParameter;

struct ShaderOverridesView;
class Shader;

namespace RG
{
    enum class ResourceAccessFlags
    {
        None = 0,

        Vertex      = BIT(1),
        Pixel       = BIT(2),
        Compute     = BIT(3),

        Index       = BIT(4),
        Indirect    = BIT(5),
        Conditional = BIT(6),
        
        Attribute   = BIT(7),
        Uniform     = BIT(8),
        Storage     = BIT(9),

        Sampled     = BIT(10),

        Blit        = BIT(11),
        Copy        = BIT(12),

        Readback    = BIT(13),
    };
    
    CREATE_ENUM_FLAGS_OPERATORS(ResourceAccessFlags)

    template <typename T>
    struct ResourceAliasTraits {};
        
    template <>
    struct ResourceAliasTraits<Buffer>
    {
        using ResourceTraits = ResourceTraits<Buffer>;
        static bool CanAlias(const ResourceTraits::Desc& description, const ResourceTraits::Desc& other, u32 otherFrame)
        {
            bool canAlias =
                (
                !enumHasAny(description.Usage, BufferUsage::Readback)
                || otherFrame >= BUFFERED_FRAMES) &&
                description.SizeBytes == other.SizeBytes && description.Usage == other.Usage;
            
            return canAlias;
        }
        static const ResourceTraits::Desc& Description(Buffer buffer) { return Device::GetBufferDescription(buffer); }
    };

    template <>
    struct ResourceAliasTraits<Texture>
    {
        using ResourceTraits = ResourceTraits<Texture>;
        static bool CanAlias(const ResourceTraits::Desc& description, const ResourceTraits::Desc& other, u32 otherFrame)
        {
            bool canAlias =
                description.Height == other.Height &&
                description.Width == other.Width &&
                description.LayersDepth == other.LayersDepth &&
                description.Mipmaps == other.Mipmaps &&
                description.Format == other.Format &&
                description.Kind == other.Kind &&
                description.Usage == other.Usage &&
                description.MipmapFilter == other.MipmapFilter;
            if (!canAlias)
                return false;

            if (description.AdditionalViews.size() != other.AdditionalViews.size())
                return false;

            for (u32 view = 0; view < description.AdditionalViews.size(); view++)
                if (description.AdditionalViews[view] != other.AdditionalViews[view])
                    return false;

            return true;
        }
        static const ResourceTraits::Desc& Description(Texture texture) { return Device::GetImageDescription(texture); }
    };

    class RenderGraphPool
    {
        static constexpr u32 MAX_UNREFERENCED_FRAMES = 4;
    public:
        ~RenderGraphPool()
        {
            for (auto& buffer : m_Buffers.Resources)
                Device::Destroy(*buffer.Resource);
            for (auto& texture : m_Textures.Resources)
                Device::Destroy(*texture.Resource);
        }
        template <typename T>
        std::shared_ptr<T> GetResource(const typename ResourceTraits<T>::Desc& description)
        {
            if constexpr(std::is_same_v<T, Buffer>)
                return m_Buffers.GetResource(description);
            else if constexpr(std::is_same_v<T, Texture>)
                return m_Textures.GetResource(description);
            else
                static_assert(!sizeof(T), "No match for type");
            std::unreachable();
        }
        template <typename T>
        std::shared_ptr<T> AddExternalResource(const T& resource)
        {
            if constexpr(std::is_same_v<T, Buffer>)
            {
                m_ExternalBuffers.Resources.push_back({
                    .Resource = std::make_shared<T>(resource),
                    .LastFrame = 0});
                
                return m_ExternalBuffers.Resources.back().Resource;
            }
            else if constexpr(std::is_same_v<T, Texture>)
            {
                m_ExternalTextures.Resources.push_back({
                    .Resource = std::make_shared<T>(resource),
                    .LastFrame = 0});
                
                return m_ExternalTextures.Resources.back().Resource;
            }
            else
            {
                static_assert(!sizeof(T), "No match for type");
            }
            std::unreachable();   
        }
        void ClearExternals()
        {
            m_ExternalBuffers.Resources.clear();
            m_ExternalTextures.Resources.clear();
        }
        void ClearUnreferenced()
        {
            for (auto& resource : m_Buffers.Resources)
                resource.LastFrame++;
            for (auto& resource : m_Textures.Resources)
                resource.LastFrame++;

            auto unorderedRemove = [this](auto& collection, auto&& onDeleteFn)
            {
                auto toRemoveIt = std::ranges::partition(collection.Resources,
                    [](auto& r) { return r.LastFrame != MAX_UNREFERENCED_FRAMES; }).begin();

                for (auto it = toRemoveIt; it != collection.Resources.end(); it++)
                    onDeleteFn(*it->Resource);
                collection.Resources.erase(toRemoveIt, collection.Resources.end());
            };

            unorderedRemove(m_Buffers, [](Buffer buffer) { Device::Destroy(buffer); });
            unorderedRemove(m_Textures, [](Texture texture) { Device::Destroy(texture); });
        }
    private:
        template <typename T>
        struct Pool
        {
            struct Item
            {
                std::shared_ptr<T> Resource;
                // how many frames since the last usage
                u32 LastFrame{0};
            };
            std::shared_ptr<T> GetResource(const typename ResourceTraits<T>::Desc& description)
            {
                for (auto&& [resource, frame] : Resources)
                {
                    if (resource.use_count() == 1 && ResourceAliasTraits<T>::CanAlias(
                        description, ResourceAliasTraits<T>::Description(*resource), frame))
                    {
                        frame = 0;
                        
                        return resource;
                    }
                }

                // allocate new resource
                if constexpr (std::is_same_v<T, Buffer>)
                {
                    Resources.push_back({
                        .Resource = std::make_shared<T>(Device::CreateBuffer({
                            .SizeBytes = description.SizeBytes,
                            .Usage = description.Usage},
                            Device::DummyDeletionQueue())),
                        .LastFrame = 0});
                }
                else
                {
                    Resources.push_back({
                        .Resource = std::make_shared<T>(Device::CreateImage({
                            .Description = description,
                            .CalculateMipmaps = false},
                            Device::DummyDeletionQueue())),
                        .LastFrame = 0});
                }
                
                return Resources.back().Resource;
            }
            std::vector<Item> Resources;
        };

        Pool<Buffer> m_Buffers;
        Pool<Texture> m_Textures;
        Pool<Buffer> m_ExternalBuffers;
        Pool<Texture> m_ExternalTextures;
    };

    class Graph
    {
        friend class Resources;
    public:
        Graph();
        ~Graph();

        Resource SetBackbuffer(Texture texture);
        Resource GetBackbuffer() const;
        
        template <typename PassData, typename SetupFn, typename CallbackFn>
        Pass& AddRenderPass(const PassName& passName, SetupFn&& setup, CallbackFn&& callback);

        Resource AddExternal(const std::string& name, Buffer buffer);
        Resource AddExternal(const std::string& name, Texture texture);
        Resource AddExternal(const std::string& name, ImageUtils::DefaultTexture texture);
        Resource AddExternal(const std::string& name, Texture texture, ImageUtils::DefaultTexture fallback);
        Resource Export(Resource resource, std::shared_ptr<Buffer>* buffer, bool force = false);
        Resource Export(Resource resource, std::shared_ptr<Texture>* texture, bool force = false);
        Resource CreateResource(const std::string& name, const GraphBufferDescription& description);
        Resource CreateResource(const std::string& name, const GraphTextureDescription& description);
        Resource Read(Resource resource, ResourceAccessFlags readFlags);
        Resource Write(Resource resource, ResourceAccessFlags writeFlags);
        Resource RenderTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore);
        Resource RenderTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore,
            const ColorClearValue& clearColor);
        Resource RenderTarget(Resource resource, ImageSubresourceDescription subresource,
            AttachmentLoad onLoad, AttachmentStore onStore, const ColorClearValue& clearColor);
        Resource DepthStencilTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore);
        Resource DepthStencilTarget(Resource resource,
            AttachmentLoad onLoad, AttachmentStore onStore,
            f32 clearDepth, u32 clearStencil = 0);
        Resource DepthStencilTarget(Resource resource,
            AttachmentLoad onLoad, AttachmentStore onStore,
            std::optional<DepthBias> depthBias, f32 clearDepth, u32 clearStencil = 0);
        Resource DepthStencilTarget(Resource resource, ImageSubresourceDescription subresource,
            AttachmentLoad onLoad, AttachmentStore onStore,
            std::optional<DepthBias> depthBias, f32 clearDepth, u32 clearStencil = 0);
        void HasSideEffect();
 
        template <typename T>
        void Upload(Resource buffer, T&& data, u64 bufferOffset = 0);

        const BufferDescription& GetBufferDescription(Resource buffer);
        const TextureDescription& GetTextureDescription(Resource texture);

        const DescriptorArenaAllocators& GetArenaAllocators() const { return *m_ArenaAllocators; }
        DescriptorArenaAllocators& GetArenaAllocators() { return *m_ArenaAllocators; }
        Blackboard& GetBlackboard() { return m_Blackboard; }
        const GlobalResources& GetGlobalResources() const { return m_Blackboard.Get<GlobalResources>(); }

        DeletionQueue& GetFrameDeletionQueue() const { ASSERT(m_FrameDeletionQueue) return *m_FrameDeletionQueue; }
        DeletionQueue& GetResolutionDeletionQueue() { return m_ResolutionDeletionQueue; }
        void OnResolutionChange();
        bool ChangedResolution() const { return m_ResolutionChangedFrames > 0; }

        void Reset(FrameContext& frameContext);
        void Compile(FrameContext& frameContext);
        void Execute(FrameContext& frameContext);
        void OnCmdBegin(FrameContext& frameContext);
        void OnCmdEnd(FrameContext& frameContext);
        void SubmitPassUploads(FrameContext& frameContext);
        
        template <typename Value>
        void UpdateBlackboard(Value&& value);

        template <typename Value>
        Value& GetBlackboardValue();
        
        template <typename Value>
        Value* TryGetBlackboardValue();
        
        template <typename Value>
        Value& GetOrCreateBlackboardValue();

        void SetShader(std::string_view path) const;
        void SetShader(std::string_view path, ShaderOverridesView&& overrides) const;
        void SetShader(const Shader* shader) const;
        void SetShader(const Shader* shader, ShaderOverridesView&& overrides) const;
        void CopyShader(const Shader* shader) const;
        const Shader& GetShader() const;
        
        std::string MermaidDump() const;
        void MermaidDumpHTML(std::string_view path) const;
    private:
        void Clear();
        Resource CreateResource(const std::string& name, const BufferDescription& description);
        Resource CreateResource(const std::string& name, const TextureDescription& description);

        void PreprocessResources();
        void CullPasses();
        void BuildAdjacencyList();
        void TopologicalSort();
        std::vector<u32> CalculateLongestPath();
        std::vector<u32> CalculateRenameRemap(auto&& filterLambda);
        void CalculateResourcesLifeSpan();
        void CreatePhysicalResources();
        void ManageBarriers();
        
        std::pair<PipelineStage, PipelineAccess> InferResourceReadAccess(BufferDescription& description,
            ResourceAccessFlags readFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceReadAccess(TextureDescription& description,
            ResourceAccessFlags readFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceWriteAccess(BufferDescription& description,
            ResourceAccessFlags writeFlags);
        std::pair<PipelineStage, PipelineAccess> InferResourceWriteAccess(TextureDescription& description,
            ResourceAccessFlags writeFlags);

        ResourceTypeBase& GetResourceTypeBase(Resource resource) const;
        Resource AddOrCreateAccess(Resource resource, PipelineStage stage, PipelineAccess access);
        Resource AddAccess(ResourceAccess& resource, PipelineStage stage, PipelineAccess access);

        Pass* CurrentPass() const;
    private:
        template <typename T>
        struct ExternalResource
        {
            T* Resource{nullptr};
        };
        using ExternalTexture = ExternalResource<Texture>;
        using ExternalBuffer = ExternalResource<Buffer>;
        
        std::vector<GraphBuffer> m_Buffers;
        std::vector<GraphTexture> m_Textures;
        std::vector<std::unique_ptr<Pass>> m_RenderPasses;
        std::vector<std::vector<u32>> m_AdjacencyList;
        std::unordered_set<std::string> m_PassNameSet;
        // storing a shared_ptr we make sure that it will not be aliased
        struct TextureToExport
        {
            Resource TextureResource{};
            std::shared_ptr<Texture>* Target{nullptr};
        };
        struct BufferToExport
        {
            Resource BufferResource{};
            std::shared_ptr<Buffer>* Target{nullptr};
        };
        std::vector<TextureToExport> m_TexturesToExport;
        std::vector<BufferToExport> m_BuffersToExport;

        std::vector<Pass*> m_CurrentPassesStack{};
        Resource m_Backbuffer{};
        std::shared_ptr<GraphTexture> m_BackbufferTexture{};
        ImageLayout m_BackbufferLayout{ImageLayout::Undefined};

        RG::ResourceUploader m_ResourceUploader;
        RenderGraphPool m_Pool;
        DeletionQueue* m_FrameDeletionQueue{nullptr};
        DeletionQueue m_ResolutionDeletionQueue{};
        u32 m_ResolutionChangedFrames{0};
         
        std::unique_ptr<DescriptorArenaAllocators> m_ArenaAllocators;
        Blackboard m_Blackboard;
    };

    class Resources
    {
    public:
        using DefaultTexture = ImageUtils::DefaultTexture;
        Resources(Graph& graph)
            : m_Graph(&graph) {}

        bool IsAllocated(Resource resource) const;
        
        Buffer GetBuffer(Resource resource) const;
        Texture GetTexture(Resource resource) const;
        const TextureDescription& GetTextureDescription(Resource resource) const;
        std::pair<Texture, const TextureDescription&> GetTextureWithDescription(Resource resource) const;
        const Graph* GetGraph() const { return m_Graph; }

        template <typename Value>
        Value& GetOrCreateValue() const;
    private:
        Graph* m_Graph{nullptr};
    };

    template <typename PassData, typename SetupFn, typename CallbackFn>
    Pass& Graph::AddRenderPass(const PassName& passName, SetupFn&& setup, CallbackFn&& callback)
    {
        ASSERT(!m_PassNameSet.contains(passName.m_Name), "Pass with such name already exists")
        m_PassNameSet.emplace(passName.m_Name);
        std::unique_ptr<Pass> pass = std::make_unique<Pass>(passName);

        m_CurrentPassesStack.push_back(pass.get());
        
        PassData passData = {};
        setup(*this, passData);
        
        pass->m_ExecutionCallback = std::make_unique<Pass::ExecutionCallback<PassData, CallbackFn>>(
            passData, std::forward<CallbackFn>(callback));

        m_RenderPasses.push_back(std::move(pass));
        m_CurrentPassesStack.pop_back();
        
        return *m_RenderPasses.back().get();
    }

    template <typename T>
    void Graph::Upload(Resource buffer, T&& data, u64 bufferOffset)
    {
        m_ResourceUploader.UpdateBuffer(CurrentPass(), buffer, std::forward<T>(data), bufferOffset);
    }

    template <typename Value>
    void Graph::UpdateBlackboard(Value&& value)
    {
        GetBlackboard().Update(m_CurrentPassesStack.back()->GetNameHash(), std::forward<Value>(value));
    }

    template <typename Value>
    Value& Graph::GetBlackboardValue()
    {
        return GetBlackboard().Get<Value>(m_CurrentPassesStack.back()->GetNameHash());
    }

    template <typename Value>
    Value* Graph::TryGetBlackboardValue()
    {
        return GetBlackboard().TryGet<Value>(m_CurrentPassesStack.back()->GetNameHash());
    }

    template <typename Value>
    Value& Graph::GetOrCreateBlackboardValue()
    {
        if (!TryGetBlackboardValue<Value>())
            UpdateBlackboard(Value{});

        return GetBlackboardValue<Value>();
    }

    std::vector<u32> Graph::CalculateRenameRemap(auto&& filterLambda)
    {
        u32 remapCount = 0;
        for (auto& pass : m_RenderPasses) 
            remapCount += (u32)pass->m_Accesses.size();

        static constexpr u32 NO_MAP = std::numeric_limits<u32>::max();
        std::vector remap(remapCount, NO_MAP);
        
        for (auto& pass : m_RenderPasses)
        {
            for (auto& access : pass->m_Accesses)
            {
                Resource resource = access.m_Resource;
                if (!filterLambda(resource))
                    continue;
                
                remap[resource.Index()] = remap[resource.Index()] == NO_MAP ?
                    resource.Index() : remap[resource.Index()];
                Resource rename = GetResourceTypeBase(resource).m_Rename;
                while (rename.IsValid())
                {
                    remap[rename.Index()] = remap[rename.Index()] == NO_MAP ?
                        resource.Index() : remap[rename.Index()];
                    rename = GetResourceTypeBase(rename).m_Rename;
                }
            }
        }
        
        return remap;
    }

    template <typename Value>
    Value& Resources::GetOrCreateValue() const
    {
        return m_Graph->GetOrCreateBlackboardValue<Value>();
    }
}



