#include "RenderGraph.h"

#include <fstream>
#include <numeric>
#include <sstream>
#include <stack>
#include <unordered_set>

#include "Renderer.h"
#include "RGResourceUploader.h"
#include "cvars/CVarSystem.h"
#include "Rendering/ShaderCache.h"
#include "Rendering/Synchronization.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    Graph::Graph()
    {
        DescriptorArenaAllocator resourceAllocator = Device::CreateDescriptorArenaAllocator({
            .Kind = DescriptorAllocatorKind::Resources,
            .Residence = DescriptorAllocatorResidence::CPU,
            .UsedTypes = {DescriptorType::UniformBuffer, DescriptorType::StorageBuffer, DescriptorType::Image},
            .DescriptorCount = 8192 * 4});

        DescriptorArenaAllocator samplerAllocator = Device::CreateDescriptorArenaAllocator({
            .Kind = DescriptorAllocatorKind::Samplers,
            .Residence = DescriptorAllocatorResidence::CPU,
            .UsedTypes = {DescriptorType::Sampler},
            .DescriptorCount = 256 * 4});
        
        m_ArenaAllocators = std::make_unique<DescriptorArenaAllocators>(resourceAllocator, samplerAllocator);
    }

    Graph::~Graph()
    {
    }

    Resource Graph::SetBackbuffer(const Texture& texture)
    {
        if (m_Backbuffer.IsValid())
        {
            m_Textures[m_Backbuffer.Index()].SetPhysicalResource(m_Pool.AddExternalResource(texture));
            m_Textures[m_Backbuffer.Index()].m_Description = texture.Description();
            m_Textures[m_Backbuffer.Index()].m_Rename = {};
        }
        else
        {
            m_Backbuffer = AddExternal("graph-backbuffer", texture);
        }
        
        m_BackbufferTexture = std::make_shared<GraphTexture>(m_Textures[m_Backbuffer.Index()]);

        return m_Backbuffer;
    }

    Resource Graph::GetBackbuffer() const
    {
        ASSERT(m_Backbuffer.IsValid(), "Backbuffer is not set")

        return m_Backbuffer;
    }

    Resource Graph::AddExternal(const std::string& name, const Buffer& buffer)
    {
        Resource bufferResource = CreateResource(name, buffer.Description());
        GetResourceTypeBase(bufferResource).m_IsExternal = true;
        m_Buffers[bufferResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(buffer));

        return bufferResource;
    }

    Resource Graph::AddExternal(const std::string& name, const Texture& texture)
    {
        Resource textureResource = CreateResource(name, texture.Description());
        GetResourceTypeBase(textureResource).m_IsExternal = true;
        m_Textures[textureResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(texture));

        return textureResource;
    }

    Resource Graph::AddExternal(const std::string& name, ImageUtils::DefaultTexture texture)
    {
        return AddExternal(name, ImageUtils::DefaultTextures::GetCopy(texture, *m_FrameDeletionQueue));
    }

    Resource Graph::AddExternal(const std::string& name, const Texture* texture, ImageUtils::DefaultTexture fallback)
    {
        if (texture)
            return AddExternal(name, *texture);

        return AddExternal(name, ImageUtils::DefaultTextures::GetCopy(fallback, *m_FrameDeletionQueue));
    }

    Resource Graph::Export(Resource resource, std::shared_ptr<Buffer>* buffer, bool force)
    {
        CurrentPass()->m_CanBeCulled = false;
        
        ASSERT(resource.IsBuffer(), "Provided resource is not a buffer")
        auto it = std::ranges::find_if(m_BuffersToExport, [&buffer](auto& res) { return res.Target == buffer; });
        ASSERT(force || it == m_BuffersToExport.end(), "Buffer is already exported to by other resource")

        m_BuffersToExport.push_back({.BufferResource = resource, .Target = buffer});

        return resource;
    }

    Resource Graph::Export(Resource resource, std::shared_ptr<Texture>* texture, bool force)
    {
        CurrentPass()->m_CanBeCulled = false;
        
        ASSERT(resource.IsTexture(), "Provided resource is not a texture")
        auto it = std::ranges::find_if(m_TexturesToExport, [&texture](auto& res) { return res.Target == texture; });
        ASSERT(force || it == m_TexturesToExport.end(), "Texture is already exported to by other resource")

        m_TexturesToExport.push_back({.TextureResource = resource, .Target = texture});

        return resource;
    }

    Resource Graph::CreateResource(const std::string& name, const GraphBufferDescription& description)
    {
        // all buffers require device address
        return CreateResource(name, BufferDescription{
            .SizeBytes = description.SizeBytes,
            .Usage = BufferUsage::DeviceAddress});
    }

    Resource Graph::CreateResource(const std::string& name, const GraphTextureDescription& description)
    {
        return CreateResource(name, TextureDescription{
            .Width = description.Width,
            .Height = description.Height,
            .Layers = description.Layers,
            .Mipmaps = description.Mipmaps,
            .Format = description.Format,
            .Kind = description.Kind,
            .Usage = ImageUsage::None,
            .MipmapFilter = description.MipmapFilter,
            .AdditionalViews = description.AdditionalViews});
    }

    Resource Graph::CreateResource(const std::string& name, const BufferDescription& description)
    {
        Resource resource = Resource::Buffer((u32)m_Buffers.size());
        m_Buffers.emplace_back(name, description);

        return resource;
    }

    Resource Graph::CreateResource(const std::string& name, const TextureDescription& description)
    {
        Resource resource = Resource::Texture((u32)m_Textures.size());
        m_Textures.emplace_back(name, description);

        return resource;
    }

    Resource Graph::Read(Resource resource, ResourceAccessFlags readFlags)
    {
        std::pair<PipelineStage, PipelineAccess> stageAccess = {};
        if (resource.IsBuffer())
            stageAccess = InferResourceReadAccess(m_Buffers[resource.Index()].m_Description, readFlags);
        else
            stageAccess = InferResourceReadAccess(m_Textures[resource.Index()].m_Description, readFlags);
        auto&& [stage, access] = stageAccess;

        return AddOrCreateAccess(resource, stage, access);
    }

    Resource Graph::Write(Resource resource, ResourceAccessFlags writeFlags)
    {
        std::pair<PipelineStage, PipelineAccess> stageAccess = {};
        if (resource.IsBuffer())
            stageAccess = InferResourceWriteAccess(m_Buffers[resource.Index()].m_Description, writeFlags);
        else
            stageAccess = InferResourceWriteAccess(m_Textures[resource.Index()].m_Description, writeFlags);
        auto&& [stage, access] = stageAccess;

        if (GetResourceTypeBase(resource).m_IsExternal)
            CurrentPass()->m_CanBeCulled = false;
        
        return AddOrCreateAccess(resource, stage, access);
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return RenderTarget(resource, {}, onLoad, onStore, {});
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, const glm::vec4& clearColor)
    {
        return RenderTarget(resource, {}, onLoad, onStore, clearColor);
    }

    Resource Graph::RenderTarget(Resource resource, ImageSubresourceDescription subresource,
        AttachmentLoad onLoad, AttachmentStore onStore, const glm::vec4& clearColor)
    {
        RenderTargetAccess renderTargetAccess = {};
        
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Color;
        renderTargetAccess.m_Resource = resource;
        if (onLoad == AttachmentLoad::Load)
            renderTargetAccess.m_Resource = AddOrCreateAccess(renderTargetAccess.m_Resource,
                PipelineStage::ColorOutput, PipelineAccess::ReadColorAttachment);    
        renderTargetAccess.m_Resource = AddOrCreateAccess(renderTargetAccess.m_Resource,
            PipelineStage::ColorOutput, PipelineAccess::WriteColorAttachment);
        renderTargetAccess.m_ViewSubresource = subresource;
        renderTargetAccess.m_ClearColor = clearColor;
        renderTargetAccess.m_OnLoad = onLoad;
        renderTargetAccess.m_OnStore = onStore;

        CurrentPass()->m_RenderTargetAttachmentAccess.push_back(renderTargetAccess);
        if (GetResourceTypeBase(resource).m_IsExternal)
            CurrentPass()->m_CanBeCulled = false;
        
        return renderTargetAccess.m_Resource;
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return DepthStencilTarget(resource, onLoad, onStore, {}, 0.0, 0);
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, f32 clearDepth, u32 clearStencil)
    {
        return DepthStencilTarget(resource, {}, onLoad, onStore, {}, clearDepth, clearStencil);
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad, AttachmentStore onStore,
        std::optional<DepthBias> depthBias, f32 clearDepth, u32 clearStencil)
    {
        return DepthStencilTarget(resource, {}, onLoad, onStore, depthBias, clearDepth, clearStencil);

    }

    Resource Graph::DepthStencilTarget(Resource resource, ImageSubresourceDescription subresource,
        AttachmentLoad onLoad, AttachmentStore onStore, std::optional<DepthBias> depthBias, f32 clearDepth,
        u32 clearStencil)
    {
        DepthStencilAccess depthStencilAccess = {};
        
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Depth | ImageUsage::Stencil;
        depthStencilAccess.m_Resource = resource;
        if (onLoad == AttachmentLoad::Load)
            depthStencilAccess.m_Resource = AddOrCreateAccess(depthStencilAccess.m_Resource,
                PipelineStage::DepthEarly | PipelineStage::DepthLate, PipelineAccess::ReadDepthStencilAttachment);    
        depthStencilAccess.m_Resource = AddOrCreateAccess(depthStencilAccess.m_Resource,
            PipelineStage::DepthEarly | PipelineStage::DepthLate, PipelineAccess::WriteDepthStencilAttachment);
        depthStencilAccess.m_ViewSubresource = subresource;
        depthStencilAccess.m_ClearDepth = clearDepth;
        depthStencilAccess.m_ClearStencil = clearStencil;
        depthStencilAccess.m_OnLoad = onLoad;
        depthStencilAccess.m_OnStore = onStore;
        depthStencilAccess.m_DepthBias = depthBias;
        depthStencilAccess.m_IsDepthOnly = m_Textures[resource.Index()].m_Description.Format == Format::D32_FLOAT;

        CurrentPass()->m_DepthStencilAccess = depthStencilAccess;
        if (GetResourceTypeBase(resource).m_IsExternal)
            CurrentPass()->m_CanBeCulled = false;

        return depthStencilAccess.m_Resource;
    }

    void Graph::HasSideEffect()
    {
        CurrentPass()->m_CanBeCulled = false;
    }

    const BufferDescription& Graph::GetBufferDescription(Resource buffer)
    {
        ASSERT(buffer.IsBuffer(), "Provided resource is not a buffer")

        return m_Buffers[buffer.Index()].m_Description;
    }
    
    const TextureDescription& Graph::GetTextureDescription(Resource texture)
    {
        ASSERT(texture.IsTexture(), "Provided resource is not a texture")
        
        return m_Textures[texture.Index()].m_Description;
    }

    void Graph::Clear()
    {
        CPU_PROFILE_FRAME("RenderGraph.Clear")
        for (auto& pass: m_RenderPasses)
        {
            pass->m_Barriers.clear();
            pass->m_SplitBarriersToSignal.clear();
            pass->m_SplitBarriersToWait.clear();
            
            pass->m_BarrierDependencyInfos.clear();
            pass->m_SplitBarrierSignalInfos.clear();
            pass->m_SplitBarrierWaitInfos.clear();

            for (auto& resourceAccess : pass->m_Accesses)
            {
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);
                resourceTypeBase.m_FirstAccess = resourceTypeBase.m_LastAccess = ResourceTypeBase::NON_INDEX;
            }
        }
    }

    void Graph::OnResolutionChange()
    {
        m_ResolutionDeletionQueue.Flush();
        m_ResolutionChangedFrames = BUFFERED_FRAMES;
    }

    void Graph::Reset(FrameContext& frameContext)
    {
        m_ResolutionChangedFrames = std::max(1u, m_ResolutionChangedFrames) - 1u;
        m_FrameDeletionQueue = &frameContext.DeletionQueue;
        
        m_Buffers.clear();
        m_Textures.clear();
        m_RenderPasses.clear();
        m_Pool.ClearExternals();
        m_Pool.ClearUnreferenced();
        m_BuffersToExport.clear();
        m_TexturesToExport.clear();
        m_PassNameSet.clear();

        if (m_Backbuffer.IsValid())
            m_Textures.push_back(*m_BackbufferTexture);
    }

    void Graph::Compile(FrameContext& frameContext)
    {
        CPU_PROFILE_FRAME("RenderGraph.Compile")
        
        Clear();
        
        PreprocessResources();

        BuildAdjacencyList();

        // todo: fix me I am weird
        //TopologicalSort();

        CullPasses();

        CalculateResourcesLifeSpan();

        CreatePhysicalResources();        

        ManageBarriers();
    }

    void Graph::Execute(FrameContext& frameContext)
    {
        OnCmdBegin(frameContext);

        Resources resources = {*this};
        for (auto& pass : m_RenderPasses)
        {
            m_CurrentPassesStack.push_back(pass.get());

            /* submit everything gathered at `setup` stage */
            SubmitPassUploads(frameContext);
            
            for (auto& barrier : pass->m_Barriers)
                RenderCommand::WaitOnBarrier(frameContext.Cmd, barrier);
            for (auto& splitWait : pass->m_SplitBarriersToWait)
                RenderCommand::WaitOnSplitBarrier(frameContext.Cmd, splitWait.Barrier, splitWait.Dependency);
            
            if (pass->m_IsRasterizationPass)
            {
                glm::uvec2 resolution = pass->m_RenderTargetAttachmentAccess.empty() ?
                    glm::uvec2{
                        m_Textures[pass->m_DepthStencilAccess->m_Resource.Index()].m_Description.Width,
                        m_Textures[pass->m_DepthStencilAccess->m_Resource.Index()].m_Description.Height
                    } :
                    glm::uvec2{
                        m_Textures[pass->m_RenderTargetAttachmentAccess.front().m_Resource.Index()].m_Description.Width,
                        m_Textures[pass->m_RenderTargetAttachmentAccess.front().m_Resource.Index()].m_Description.Height
                    };
                    
                std::optional<DepthBias> depthBias{};

                std::vector<RenderingAttachment> attachments;
                attachments.reserve(pass->m_RenderTargetAttachmentAccess.size() +
                    (u32)pass->m_DepthStencilAccess.has_value());
                for (auto& target : pass->m_RenderTargetAttachmentAccess)
                    attachments.push_back(RenderingAttachment::Builder()
                        .ClearValue(target.m_ClearColor)
                        .SetType(RenderingAttachmentType::Color)
                        .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                        .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource,
                            ImageLayout::ColorAttachment)
                        .View(target.m_ViewSubresource)
                        .Build(*m_FrameDeletionQueue));
                if (pass->m_DepthStencilAccess.has_value())
                {
                    auto& target = *pass->m_DepthStencilAccess;

                    ImageLayout layout = target.m_IsDepthOnly ?
                        ImageLayout::DepthAttachment : ImageLayout::DepthStencilAttachment;
                    
                    attachments.push_back(RenderingAttachment::Builder()
                       .ClearValue(target.m_ClearDepth, target.m_ClearStencil)
                       .SetType(RenderingAttachmentType::Depth)
                       .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                       .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource, layout)
                       .View(target.m_ViewSubresource)
                       .Build(*m_FrameDeletionQueue));

                    /* add a depth bias, if depth target was created with it */
                    if (target.m_DepthBias.has_value())
                        depthBias = *target.m_DepthBias;
                }
                RenderingInfo::Builder renderingInfoBuilder = RenderingInfo::Builder()
                    .SetResolution(resolution);
                for (auto& attachment : attachments)
                    renderingInfoBuilder.AddAttachment(attachment);

                RenderingInfo renderingInfo = renderingInfoBuilder.Build(*m_FrameDeletionQueue);
                
                RenderCommand::BeginRendering(frameContext.Cmd, renderingInfo);

                /* set dynamic states */
                RenderCommand::SetViewport(frameContext.Cmd, resolution);
                RenderCommand::SetScissors(frameContext.Cmd, {0, 0}, resolution);
                if (depthBias.has_value())
                    RenderCommand::SetDepthBias(frameContext.Cmd, *depthBias);
                
                pass->Execute(frameContext, resources);

                RenderCommand::EndRendering(frameContext.Cmd);
            }
            else
            {
                pass->Execute(frameContext, resources);
            }
            
            for (auto& splitSignal : pass->m_SplitBarriersToSignal)
                RenderCommand::SignalSplitBarrier(frameContext.Cmd, splitSignal.Barrier, splitSignal.Dependency);

            m_CurrentPassesStack.pop_back();
        }

        if (!m_Backbuffer.IsValid())
            return;
        
        // transition backbuffer to the layout that swapchain expects
        const Texture& backbuffer = *m_Textures[m_Backbuffer.Index()].m_Resource;
        ImageSubresource backbufferSubresource = backbuffer.Subresource(0, 1, 0, 1);
        LayoutTransitionInfo backbufferTransition = {
            .ImageSubresource = backbufferSubresource,
            .SourceStage = PipelineStage::AllCommands,
            .DestinationStage = PipelineStage::Bottom,
            .SourceAccess = PipelineAccess::WriteAll | PipelineAccess::ReadAll,
            .DestinationAccess = PipelineAccess::None,
            .OldLayout = m_BackbufferLayout,
            .NewLayout = ImageLayout::Source};
        DependencyInfo transitionDependency = DependencyInfo::Builder()
            .LayoutTransition(backbufferTransition)
            .Build(*m_FrameDeletionQueue);
        RenderCommand::WaitOnBarrier(frameContext.Cmd, transitionDependency);
    }

    void Graph::OnCmdBegin(FrameContext& frameContext)
    {
        GetArenaAllocators().Bind(frameContext.Cmd, frameContext.FrameNumber);
    }

    void Graph::OnCmdEnd(FrameContext& frameContext)
    {
        SubmitPassUploads(frameContext);
    }

    void Graph::SubmitPassUploads(FrameContext& frameContext)
    {
        /* avoid barriers if there is no data to upload */
        if (!m_ResourceUploader.HasUploads(CurrentPass()))
            return;

        RenderCommand::WaitOnBarrier(frameContext.Cmd, DependencyInfo::Builder()
            .ExecutionDependency({
                .SourceStage = PipelineStage::AllCommands,
                .DestinationStage = PipelineStage::AllTransfer})
            .Build(frameContext.DeletionQueue));
        m_ResourceUploader.Upload(CurrentPass(), Resources{*this}, *frameContext.ResourceUploader);
        frameContext.ResourceUploader->SubmitUpload(frameContext.Cmd);
        RenderCommand::WaitOnBarrier(frameContext.Cmd, DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = PipelineStage::AllTransfer,
                .DestinationStage = PipelineStage::AllCommands,
                .SourceAccess = PipelineAccess::WriteAll,
                .DestinationAccess = PipelineAccess::ReadAll})
            .Build(frameContext.DeletionQueue));
    }

    void Graph::SetShader(std::string_view path) const
    {
        ShaderCache::Register(CurrentPass()->GetNameString(), path, {});
    }

    void Graph::SetShader(std::string_view path, ShaderOverridesView&& overrides) const
    {
        ShaderCache::Register(CurrentPass()->GetNameString(), path, std::move(overrides));
    }

    void Graph::SetShader(const Shader* shader) const
    {
        ShaderCache::Register(CurrentPass()->GetNameString(), shader, {});
    }

    void Graph::SetShader(const Shader* shader, ShaderOverridesView&& overrides) const
    {
        ShaderCache::Register(CurrentPass()->GetNameString(), shader, std::move(overrides));
    }

    void Graph::CopyShader(const Shader* shader) const
    {
        ShaderCache::Register(CurrentPass()->GetNameString(), shader, shader->CopyOverrides());
    }

    const Shader& Graph::GetShader() const
    {
        return ShaderCache::Get(CurrentPass()->GetNameString());
    }

    void Graph::PreprocessResources()
    {
        CPU_PROFILE_FRAME("RenderGraph.Preprocess")
        for (auto& buffer : m_Buffers)
            buffer.m_Description.Usage |= BufferUsage::Destination;
    }

    void Graph::CullPasses()
    {
        CPU_PROFILE_FRAME("RenderGraph.Cull")
        std::vector passRefCount(m_RenderPasses.size(), 0u);
        struct ResourceRefInfo
        {
            u32 Count{0};
            u32 Producer{0};
        };
        struct ProducerInfo
        {
            std::vector<u32> ReadBuffers;
            std::vector<u32> ReadTextures;
        };
        std::vector bufferRefCount(m_Buffers.size(), ResourceRefInfo{});
        std::vector textureRefCount(m_Textures.size(), ResourceRefInfo{});
        std::vector producerMap(m_RenderPasses.size(), ProducerInfo{});

        static constexpr u32 NO_MAP = std::numeric_limits<u32>::max();
        std::vector bufferRemap(m_Buffers.size(), NO_MAP);
        std::vector textureRemap(m_Textures.size(), NO_MAP);

        for (auto& pass : m_RenderPasses)
        {
            for (auto& access : pass->m_Accesses)
            {
                Resource resource = access.m_Resource;
                u32 index = resource.Index();
                if (resource.IsBuffer())
                {
                    bufferRemap[index] = bufferRemap[index] == NO_MAP ?
                        index : bufferRemap[index];
                    if (m_Buffers[index].m_Rename.IsValid())
                        bufferRemap[m_Buffers[index].m_Rename.Index()] = bufferRemap[index];
                }
                else
                {
                    textureRemap[index] = textureRemap[index] == NO_MAP ?
                        resource.Index() : textureRemap[index];
                    if (m_Textures[index].m_Rename.IsValid())
                        textureRemap[m_Textures[index].m_Rename.Index()] = textureRemap[index];
                }
            }
        }

        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            for (auto& access : m_RenderPasses[passIndex]->m_Accesses)
            {
                Resource resource = access.m_Resource;
                u32 index = resource.Index();
                if (resource.IsBuffer())
                {
                    if (ResourceAccess::HasWriteAccess(access.m_Access))
                    {
                        passRefCount[passIndex]++;
                        bufferRefCount[bufferRemap[index]].Producer = passIndex;
                    }
                    if (ResourceAccess::HasReadAccess(access.m_Access))
                    {
                        bufferRefCount[bufferRemap[index]].Count++;
                        producerMap[passIndex].ReadBuffers.push_back(bufferRemap[index]);
                    }
                }
                else
                {
                    if (ResourceAccess::HasWriteAccess(access.m_Access))
                    {
                        passRefCount[passIndex]++;
                        textureRefCount[textureRemap[index]].Producer = passIndex;
                    }
                    if (ResourceAccess::HasReadAccess(access.m_Access))
                    {
                        textureRefCount[textureRemap[index]].Count++;
                        producerMap[passIndex].ReadTextures.push_back(textureRemap[index]);
                    }
                }
            }
        }
        if (m_Backbuffer.IsValid())
            textureRefCount[m_Backbuffer.Index()].Count++;

        std::stack<u32> unrefBuffers;
        std::stack<u32> unrefTextures;

        auto initStack = [](auto& stack, auto& refCounts)
        {
            for (u32 resourceIndex = 0; resourceIndex < refCounts.size(); resourceIndex++)
                if (refCounts[resourceIndex].Count == 0)
                    stack.push(resourceIndex);
        };
        
        auto onProducerCull = [](auto& refCounts, auto& references, auto& stack)
        {
            for (u32 resourceIndex = 0; resourceIndex < references.size(); resourceIndex++)
            {
                refCounts[references[resourceIndex]].Count--;
                if (refCounts[references[resourceIndex]].Count == 0)
                    stack.push(references[resourceIndex]);
            }
        };
        
        initStack(unrefBuffers, bufferRefCount);
        initStack(unrefTextures, textureRefCount);

        while (!unrefBuffers.empty() || !unrefTextures.empty())
        {
            u32 index;
            u32 producer;
            if (!unrefBuffers.empty())
            {
                index = unrefBuffers.top(); unrefBuffers.pop();
                producer = bufferRefCount[index].Producer;
            }
            else
            {
                index = unrefTextures.top(); unrefTextures.pop();
                producer = textureRefCount[index].Producer;
            }
            passRefCount[producer]--;
            if (passRefCount[producer] == 0)
            {
                onProducerCull(bufferRefCount, producerMap[producer].ReadBuffers, unrefBuffers);
                onProducerCull(textureRefCount, producerMap[producer].ReadTextures, unrefTextures);
            }
        }

        // todo: the actual culling. Also this algorithm can be simplified if we account for non-cullable passes
        //for (u32 passIndex = 0; passIndex < passRefCount.size(); passIndex++)
        //    if (passRefCount[passIndex] == 0 && m_RenderPasses[passIndex]->m_CanBeCulled)
        //        LOG("TO BE CULLED: {}", m_RenderPasses[passIndex]->m_Name.m_Name);
    }

    // todo: use for barriers
    void Graph::BuildAdjacencyList()
    {
        CPU_PROFILE_FRAME("RenderGraph.Adjacency")
        m_AdjacencyList = std::vector(m_RenderPasses.size(), std::vector<u32>{});
        // beautiful 
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
            for (u32 otherIndex = passIndex + 1; otherIndex < m_RenderPasses.size(); otherIndex++)
                for (auto& access : m_RenderPasses[passIndex]->m_Accesses)
                    for (auto& otherAccess : m_RenderPasses[otherIndex]->m_Accesses)
                        if (access.m_Resource == otherAccess.m_Resource &&
                            ResourceAccess::HasWriteAccess(access.m_Access))
                                m_AdjacencyList[passIndex].push_back(otherIndex);
    }

    void Graph::TopologicalSort()
    {
        std::vector<u32> sortedIndices;
        sortedIndices.reserve(m_RenderPasses.size());
        struct Mark
        {
            bool Permanent{false};
            bool Temporary{false};
        };
        std::vector<Mark> marks(m_RenderPasses.size());
        std::vector<u32> unprocessed(m_RenderPasses.size());
        std::ranges::generate(unprocessed.begin(), unprocessed.end(),
            [n = (u32)m_RenderPasses.size() - 1]() mutable { return n--; });

        // assume that pass 0 has no dependency
        auto dfsSort = [&sortedIndices, &marks, this](u32 index)
        {
            auto dfsSortRecursive = [&sortedIndices, &marks, this](u32 index, auto& dfs)
            {
                if (marks[index].Permanent)
                    return;
                ASSERT(!marks[index].Temporary, "Circular dependency in graph (node {})", index)

                marks[index].Temporary = true;
                for (u32 adjacent : m_AdjacencyList[index])
                    dfs(adjacent, dfs);

                marks[index].Temporary = false;
                marks[index].Permanent = true;
                sortedIndices.push_back(index);
            };

            dfsSortRecursive(index, dfsSortRecursive);
        };

        while (!unprocessed.empty())
        {
            u32 toProcess = unprocessed.back(); unprocessed.pop_back();
            if (!marks[toProcess].Permanent)
                dfsSort(toProcess);
        }

        std::ranges::reverse(sortedIndices);
        for (u32 index = 0; index < sortedIndices.size(); index++)
        {
            u32 current = index;
            u32 next = sortedIndices[current];
            while (next != index)
            {
                std::swap(m_RenderPasses[current], m_RenderPasses[next]);
                std::swap(m_AdjacencyList[current], m_AdjacencyList[next]);
                sortedIndices[current] = current;
                current = next;
                next = sortedIndices[next];
            }
            sortedIndices[current] = current;
        }
    }

    // todo: use for barriers
    std::vector<u32> Graph::CalculateLongestPath()
    {
        std::vector<u32> longestPath(m_RenderPasses.size());
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
            for (u32 adjacentIndex : m_AdjacencyList[passIndex])
                if (longestPath[adjacentIndex] < longestPath[passIndex] + 1)
                    longestPath[adjacentIndex] = longestPath[passIndex] + 1;

        return longestPath;
    }

    void Graph::CalculateResourcesLifeSpan()
    {
        CPU_PROFILE_FRAME("RenderGraph.ResourcesLifeSpan")
        // determine resources usage spans, assuming the passes are ordered correctly
        // todo: pass reordering
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            bool isRasterizationPass = true;
            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                isRasterizationPass = isRasterizationPass && !enumHasAny(
                    resourceAccess.m_Stage, PipelineStage::ComputeShader);
                
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);
                resourceTypeBase.m_FirstAccess = resourceTypeBase.m_FirstAccess == ResourceTypeBase::NON_INDEX ?
                    passIndex : resourceTypeBase.m_FirstAccess;
                resourceTypeBase.m_LastAccess = passIndex;

                ASSERT(resourceAccess.m_Stage != PipelineStage::None,
                    "Stage is not inferrable for {}", resourceTypeBase.m_Name)
                ASSERT(resourceAccess.m_Access != PipelineAccess::None,
                    "Access is not inferrable for {}", resourceTypeBase.m_Name)
                ASSERT(resourceAccess.m_Resource.IsBuffer() ?
                    m_Buffers[resourceAccess.m_Resource.Index()].m_Description.Usage != BufferUsage::None :
                    m_Textures[resourceAccess.m_Resource.Index()].m_Description.Usage != ImageUsage::None,
                    "Usage is not inferrable for {}", resourceTypeBase.m_Name)
            }

            Pass& pass = *m_RenderPasses[passIndex];
            pass.m_IsRasterizationPass = isRasterizationPass &&
                !(pass.m_RenderTargetAttachmentAccess.empty() && !pass.m_DepthStencilAccess.has_value());
        }
    }

    void Graph::CreatePhysicalResources()
    {
        CPU_PROFILE_FRAME("RenderGraph.Allocation.Deallocation")
        auto handleAllocation = [](auto& collection, u32 index, bool needsAllocation, bool hasRename,
            const auto& allocFn)
        {
            auto& graphResource = collection[index];
            if (needsAllocation && graphResource.m_Resource == nullptr)
            {
                // only the last rename has a full info on it's usage
                auto* resource = &graphResource;
                while (resource->m_Rename.IsValid())
                    resource = &collection[resource->m_Rename.Index()];
                graphResource.SetPhysicalResource(allocFn(*resource));
            }
            if (hasRename)
                collection[graphResource.m_Rename.Index()].SetPhysicalResource(graphResource.m_ResourceRef);
        };
        auto handleDeallocation = [](auto& collection, u32 index)
        {
            collection[index].ReleaseResource();
        };
        std::vector<Resource> toDeallocate;
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            toDeallocate.clear();
            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                Resource resource = resourceAccess.m_Resource;
                
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resource);

                /* if a resource has a 'Rename', the physical resource will be shared
                 * it means, that upon allocation the acquired physical resource should be also referenced
                 * in all the renames (which is a linked list),
                 * and the deallocation shall be delayed until resource is the last in the chain of renames
                 */
                bool hasRename = resourceTypeBase.m_Rename.IsValid();
                
                bool needsAllocation = resourceTypeBase.m_FirstAccess == passIndex &&
                    !resourceTypeBase.m_IsExternal;
                bool needsDeallocation = resourceTypeBase.m_LastAccess == passIndex &&
                    !resourceTypeBase.m_IsExternal && !hasRename;
                // delay deallocation, so we don't incorrectly alias an active resource
                if (needsDeallocation)
                    toDeallocate.push_back(resource);
                
                if (resource.IsBuffer())
                    handleAllocation(m_Buffers, resource.Index(), needsAllocation, hasRename,
                        [this](auto& res){ return m_Pool.GetResource<Buffer>(res.m_Description); });
                else
                    handleAllocation(m_Textures, resource.Index(), needsAllocation, hasRename,
                        [this](auto& res){ return m_Pool.GetResource<Texture>(res.m_Description); });
            }
            for (Resource resource : toDeallocate)
            {
                if (resource.IsBuffer())
                    handleDeallocation(m_Buffers, resource.Index());
                else
                    handleDeallocation(m_Textures, resource.Index());
            }
                
        }
        
        for (auto& buffer : m_BuffersToExport)
            *buffer.Target = m_Buffers[buffer.BufferResource.Index()].m_ResourceRef; 
        for (auto& texture : m_TexturesToExport)
            *texture.Target = m_Textures[texture.TextureResource.Index()].m_ResourceRef; 
    }

    void Graph::ManageBarriers()
    {
        CPU_PROFILE_FRAME("RenderGraph.ManageBarriers")
        std::vector longestPath = CalculateLongestPath();

        std::vector bufferRemap = CalculateRenameRemap([](Resource resource) { return resource.IsBuffer(); });
        std::vector textureRemap = CalculateRenameRemap([](Resource resource) { return resource.IsTexture(); });

        static constexpr u32 NO_PASS = std::numeric_limits<u32>::max();
        // data needed for barriers and layout transitions
        struct TextureTransitionInfo
        {
            Resource Texture;
            PipelineStage SourceStage;
            PipelineStage DestinationStage;
            PipelineAccess SourceAccess;   
            PipelineAccess DestinationAccess;
            ImageLayout OldLayout;
            ImageLayout NewLayout;
            u32 SourcePass{NO_PASS};
            u32 DestinationPass{NO_PASS};

            TextureTransitionInfo Merge(const TextureTransitionInfo& other)
            {
                TextureTransitionInfo info = *this;
                info.DestinationStage = other.DestinationStage;
                info.DestinationAccess = other.DestinationAccess;
                info.NewLayout = other.NewLayout;
                info.DestinationPass = other.DestinationPass;

                return info;
            }
        };
        struct LayoutInfo
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageLayout Layout{ImageLayout::Undefined};
            u32 PassIndex{NO_PASS};
        };
        std::vector currentLayouts(m_Textures.size(), LayoutInfo{});
        enum class AccessType
        {
            None, Read, Write
            // Mixed is also possible, but we don't actually need to do anything special
            // to handle it
        };
        struct AccessInfo
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            AccessType Type{AccessType::None};
            u32 PassIndex{NO_PASS};
        };
        std::vector currentBufferAccess(m_Buffers.size(), AccessInfo{});
        std::vector currentTextureAccess(m_Textures.size(), AccessInfo{});

        auto inferDesiredLayout = [this](const ResourceAccess& access, const GraphTexture& texture)
        {
            if (enumHasAny(access.m_Access, PipelineAccess::ReadTransfer | PipelineAccess::WriteTransfer))
            {
                ASSERT(
                    (enumHasAny(access.m_Access, PipelineAccess::ReadTransfer) ||
                     enumHasAny(access.m_Access, PipelineAccess::WriteTransfer)) &&
                    !enumHasAll(access.m_Access, PipelineAccess::ReadTransfer | PipelineAccess::WriteTransfer),
                    "Cannot mix transfer accesses with any other type of access")
                if (enumHasAny(access.m_Access, PipelineAccess::ReadTransfer))
                    return ImageLayout::Source;
                return ImageLayout::Destination;
            }
            if (ResourceAccess::HasWriteAccess(access.m_Access))
            {
                if (enumHasAny(access.m_Stage, PipelineStage::ComputeShader))
                    return ImageLayout::General;
                if (enumHasAny(access.m_Access, PipelineAccess::WriteColorAttachment))
                    return ImageLayout::ColorAttachment;
                if (enumHasAny(access.m_Access, PipelineAccess::WriteDepthStencilAttachment))
                    return texture.m_Description.Format == Format::D32_FLOAT ?
                        ImageLayout::DepthAttachment : ImageLayout::DepthStencilAttachment;
                return ImageLayout::Attachment;
            }
            if (enumHasAny(access.m_Access, PipelineAccess::ReadDepthStencilAttachment) ||
                texture.m_Description.Format == Format::D32_FLOAT)
                return texture.m_Description.Format == Format::D32_FLOAT ?
                    ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly;
            return ImageLayout::Readonly;
        };
        auto updateAndPropagate = [this](auto& collection, Resource resource, const auto& updateData)
        {
            collection[resource.Index()] = updateData;
            Resource rename = GetResourceTypeBase(resource).m_Rename;
            while (rename.IsValid())
            {
                collection[rename.Index()] = updateData;
                rename = GetResourceTypeBase(rename).m_Rename;
            }
        };
        auto addExecutionDependency = [this](Pass& pass, Resource resource, const ExecutionDependencyInfo& dependency)
        {
            pass.m_Barriers.push_back(
               DependencyInfo::Builder()
                   .ExecutionDependency(dependency)
                   .Build(*m_FrameDeletionQueue));
            pass.m_BarrierDependencyInfos.push_back({
                .Resource = resource,
                .ExecutionDependency = dependency});
        };
        auto addExecutionSplitBarrier = [this](Pass& passSignal, Pass& passWait, Resource resource,
            const ExecutionDependencyInfo& dependency)
        {
            DependencyInfo dependencyInfo = DependencyInfo::Builder()
                   .ExecutionDependency(dependency)
                   .Build(*m_FrameDeletionQueue);
            SplitBarrier splitBarrier = SplitBarrier::Builder().Build(*m_FrameDeletionQueue);
            passSignal.m_SplitBarriersToSignal.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});
            passWait.m_SplitBarriersToWait.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});

            passSignal.m_SplitBarrierSignalInfos.push_back({
                .Resource = resource,
                .ExecutionDependency = dependency});
            passWait.m_SplitBarrierWaitInfos.push_back({
                .Resource = resource,
                .ExecutionDependency = dependency});
        };
        auto addMemoryDependency = [this](Pass& pass, Resource resource, const MemoryDependencyInfo& dependency)
        {
            pass.m_Barriers.push_back(
                DependencyInfo::Builder()
                    .SetFlags(resource.IsTexture() ?
                        PipelineDependencyFlags::ByRegion : PipelineDependencyFlags::None)
                    .MemoryDependency(dependency)
                    .Build(*m_FrameDeletionQueue));
            pass.m_BarrierDependencyInfos.push_back({
                .Resource = resource,
                .MemoryDependency = dependency});
        };
        auto addMemorySplitBarrier = [this](Pass& passSignal, Pass& passWait, Resource resource,
            const MemoryDependencyInfo& dependency)
        {
            DependencyInfo dependencyInfo = DependencyInfo::Builder()
                .MemoryDependency(dependency)
                .Build(*m_FrameDeletionQueue);
            SplitBarrier splitBarrier = SplitBarrier::Builder().Build(*m_FrameDeletionQueue);
            passSignal.m_SplitBarriersToSignal.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});
            passWait.m_SplitBarriersToWait.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});

            passSignal.m_SplitBarrierSignalInfos.push_back({
                .Resource = resource,
                .MemoryDependency = dependency});
            passWait.m_SplitBarrierWaitInfos.push_back({
                .Resource = resource,
                .MemoryDependency = dependency});
        };
        auto addLayoutDependency = [this](Pass& pass, Resource resource, const LayoutTransitionInfo& dependency)
        {
            pass.m_Barriers.push_back(
                DependencyInfo::Builder()
                    .SetFlags(PipelineDependencyFlags::ByRegion)
                    .LayoutTransition(dependency)
                    .Build(*m_FrameDeletionQueue));
            pass.m_BarrierDependencyInfos.push_back({
                .Resource = resource,
                .LayoutTransition = dependency});
        };
        auto addLayoutSplitBarrier = [this](Pass& passSignal, Pass& passWait, Resource resource,
            const LayoutTransitionInfo& dependency)
        {
            DependencyInfo dependencyInfo = DependencyInfo::Builder()
                .LayoutTransition(dependency)
                .Build(*m_FrameDeletionQueue);
            SplitBarrier splitBarrier = SplitBarrier::Builder().Build(*m_FrameDeletionQueue);
            passSignal.m_SplitBarriersToSignal.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});
            passWait.m_SplitBarriersToWait.push_back({
                .Dependency = dependencyInfo,
                .Barrier = splitBarrier});

            passSignal.m_SplitBarrierSignalInfos.push_back({
                .Resource = resource,
                .LayoutTransition = dependency});
            passWait.m_SplitBarrierWaitInfos.push_back({
                .Resource = resource,
                .LayoutTransition = dependency});
        };
        auto addLayoutTransition = [this, &addLayoutDependency, &addLayoutSplitBarrier, longestPath]
            (const TextureTransitionInfo& transition)
        {
            auto& texture = m_Textures[transition.Texture.Index()];
                
            ImageSubresource subresource = texture.m_Resource->Subresource();
            LayoutTransitionInfo layoutTransitionInfo = {
                .ImageSubresource = subresource,
                .SourceStage = transition.SourceStage,
                .DestinationStage = transition.DestinationStage,
                .SourceAccess = transition.SourceAccess,
                .DestinationAccess = transition.DestinationAccess,
                .OldLayout = transition.OldLayout,
                .NewLayout = transition.NewLayout};

            if (longestPath[transition.DestinationPass] - longestPath[transition.SourcePass] > 1)
                addLayoutSplitBarrier(
                    *m_RenderPasses[transition.SourcePass], *m_RenderPasses[transition.DestinationPass],
                    transition.Texture, layoutTransitionInfo);
            else
                addLayoutDependency(*m_RenderPasses[transition.DestinationPass],
                    transition.Texture, layoutTransitionInfo);
        };
        
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = m_RenderPasses[passIndex];
            std::vector<TextureTransitionInfo> layoutTransitions;
            for (auto& resourceAccess : pass->m_Accesses)
            {
                Resource resource = resourceAccess.m_Resource;

                AccessType currentAccess = resource.IsBuffer() ?
                    currentBufferAccess[resource.Index()].Type : currentTextureAccess[resource.Index()].Type;
                AccessType accessType = ResourceAccess::HasWriteAccess(resourceAccess.m_Access) ?
                    AccessType::Write : AccessType::Read;

                bool shouldHandleBarriers = true;
                
                if (GetResourceTypeBase(resource).m_FirstAccess == passIndex && currentAccess == AccessType::None)
                {
                    AccessInfo accessInfo = {
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType,
                        .PassIndex = passIndex};
                    if (resource.IsTexture())
                        currentTextureAccess[resource.Index()] = accessInfo;
                    else
                        currentBufferAccess[resource.Index()] = accessInfo;

                    shouldHandleBarriers = false;
                }

                if (resource.IsTexture())
                {
                    auto& texture = m_Textures[resource.Index()];
                    
                    // layout transitions
                    ImageLayout currentLayout = currentLayouts[resource.Index()].Layout;
                    ImageLayout desiredLayout = inferDesiredLayout(resourceAccess, texture);

                    if (currentLayout != desiredLayout)
                    {
                        layoutTransitions.push_back({
                            .Texture = resource,
                            .SourceStage = currentLayouts[resource.Index()].Stage,
                            .DestinationStage = resourceAccess.m_Stage,
                            .SourceAccess = currentLayouts[resource.Index()].Access,
                            .DestinationAccess = resourceAccess.m_Access,
                            .OldLayout = currentLayout,
                            .NewLayout = desiredLayout,
                            .SourcePass = currentLayouts[resource.Index()].PassIndex == NO_PASS ?
                                passIndex : currentLayouts[resource.Index()].PassIndex,
                            .DestinationPass = passIndex});

                        updateAndPropagate(currentLayouts, resource, LayoutInfo{
                            .Stage = resourceAccess.m_Stage,
                            .Access = resourceAccess.m_Access,
                            .Layout = desiredLayout,
                            .PassIndex = passIndex});

                        // layout change is a barrier itself, so no further processing is necessary 
                        shouldHandleBarriers = false;
                    }
                }
                
                const AccessInfo& currentAccessInfo = resource.IsBuffer() ?
                    currentBufferAccess[resource.Index()] : currentTextureAccess[resource.Index()];
                
                if (shouldHandleBarriers &&
                    currentAccessInfo.Type == AccessType::Read && accessType == AccessType::Write)
                {
                    // simple execution barrier
                    ExecutionDependencyInfo executionDependencyInfo = {
                        .SourceStage = currentAccessInfo.Stage,
                        .DestinationStage = resourceAccess.m_Stage};

                    if (longestPath[passIndex] - longestPath[currentAccessInfo.PassIndex] > 1)
                        addExecutionSplitBarrier(
                            *m_RenderPasses[currentAccessInfo.PassIndex], *pass,
                            resource, executionDependencyInfo);
                    else
                        addExecutionDependency(*pass, resource, executionDependencyInfo);
                }
                else if (shouldHandleBarriers &&
                    !(currentAccessInfo.Type == AccessType::Read && accessType == AccessType::Read))
                {
                    // memory barrier
                    MemoryDependencyInfo memoryDependencyInfo = {
                        .SourceStage = currentAccessInfo.Stage,
                        .DestinationStage = resourceAccess.m_Stage,
                        .SourceAccess = currentAccessInfo.Access,
                        .DestinationAccess = resourceAccess.m_Access};

                    if (longestPath[passIndex] - longestPath[currentAccessInfo.PassIndex] > 1)
                        addMemorySplitBarrier(
                            *m_RenderPasses[currentAccessInfo.PassIndex], *pass,
                            resource, memoryDependencyInfo);
                    else
                        addMemoryDependency(*pass, resource, memoryDependencyInfo);
                }

                if (resource.IsBuffer())
                    updateAndPropagate(currentBufferAccess, resource, AccessInfo{
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType,
                        .PassIndex = passIndex});
                else
                    updateAndPropagate(currentTextureAccess, resource, AccessInfo{
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType,
                        .PassIndex = passIndex});
            }

            // sometimes you may have redundant layout transitions
            auto mergeLayoutsOrConvertIntoBarrier =
                [&layoutTransitions, &addMemoryDependency, &addLayoutTransition](Pass& pass, u32 start, u32 end)
            {
                auto& transition = layoutTransitions[start];
                auto& otherTransition = layoutTransitions[end];
                
                ImageLayout firstLayout = transition.OldLayout;
                ImageLayout lastLayout = otherTransition.NewLayout;
                if (firstLayout == lastLayout)
                {
                    MemoryDependencyInfo memoryDependencyInfo = {
                        .SourceStage = transition.SourceStage,
                        .DestinationStage = otherTransition.DestinationStage,
                        .SourceAccess = transition.SourceAccess,
                        .DestinationAccess = otherTransition.DestinationAccess};
                    addMemoryDependency(pass, transition.Texture, memoryDependencyInfo);
                }
                else
                {
                    addLayoutTransition(transition.Merge(otherTransition));
                }
            };
            
            if (layoutTransitions.size() > 1)
            {
                std::ranges::sort(layoutTransitions, [textureRemap](auto& a, auto& b)
                {
                    return textureRemap[a.Texture.Index()] < textureRemap[b.Texture.Index()];
                });
                u32 currentIndex = 0;
                
                for (u32 index = 1; index < layoutTransitions.size(); index++)
                {
                    auto& transition = layoutTransitions[currentIndex];
                    auto& otherTransition = layoutTransitions[index];
                    
                    if (textureRemap[otherTransition.Texture.Index()] != textureRemap[transition.Texture.Index()])
                    {
                        if (index - currentIndex > 1)
                        {
                            mergeLayoutsOrConvertIntoBarrier(*pass, currentIndex, index - 1);
                            if (index == layoutTransitions.size() - 1)
                                addLayoutTransition(otherTransition);
                        }
                        else if (index == layoutTransitions.size() - 1)
                        {
                            addLayoutTransition(transition);
                            addLayoutTransition(otherTransition);
                        }
                        else
                        {
                            addLayoutTransition(transition);
                        }
                        currentIndex = index;
                    }
                }
                if (currentIndex != (u32)layoutTransitions.size() - 1)
                    mergeLayoutsOrConvertIntoBarrier(*pass, currentIndex, (u32)layoutTransitions.size() - 1);
            }
            else if (!layoutTransitions.empty())
            {
                auto& transition = layoutTransitions.front();
                addLayoutTransition(transition);
            }
        }

        if (m_Backbuffer.IsValid())
        {
            Resource backbuffer = m_Backbuffer;
            while (GetResourceTypeBase(backbuffer).m_Rename.IsValid())
                backbuffer = GetResourceTypeBase(backbuffer).m_Rename;
            m_BackbufferLayout = currentLayouts[backbuffer.Index()].Layout;
        }
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceReadAccess(BufferDescription& description,
        ResourceAccessFlags readFlags)
    {
        ASSERT(!enumHasAny(readFlags,
            ResourceAccessFlags::Blit | ResourceAccessFlags::Sampled), "Buffer read has inappropriate flags")
        
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            BufferUsage Usage{BufferUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Index,
                ResourceSubAccess{
                    .Stage = PipelineStage::IndexInput,
                    .Access = PipelineAccess::ReadIndex,
                    .Usage = BufferUsage::Index}
            },
            {
                ResourceAccessFlags::Indirect,
                ResourceSubAccess{
                    .Stage = PipelineStage::Indirect,
                    .Access = PipelineAccess::ReadIndirect,
                    .Usage = BufferUsage::Indirect}
            },
            {
                ResourceAccessFlags::Conditional,
                ResourceSubAccess{
                    .Stage = PipelineStage::ConditionalRendering,
                    .Access = PipelineAccess::ReadConditional,
                    .Usage = BufferUsage::Conditional}
            },
            {
                ResourceAccessFlags::Attribute,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexInput,
                    .Access = PipelineAccess::ReadAttribute,
                    .Usage = BufferUsage::Vertex}
            },
            {
                ResourceAccessFlags::Uniform,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadUniform,
                    .Usage = BufferUsage::Uniform}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadStorage,
                    .Usage = BufferUsage::Storage}
            },
            {
                ResourceAccessFlags::Copy,
                ResourceSubAccess{
                    .Stage = PipelineStage::Copy,
                    .Access = PipelineAccess::ReadTransfer,
                    .Usage = BufferUsage::Source}
            },
            {
                ResourceAccessFlags::Readback,
                ResourceSubAccess{
                    .Stage = PipelineStage::Host,
                    .Access = PipelineAccess::ReadHost,
                    .Usage = BufferUsage::Readback}
            },
        }; 
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;

        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(readFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }
        
        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceReadAccess(TextureDescription& description,
        ResourceAccessFlags readFlags)
    {
        ASSERT(!enumHasAny(readFlags,
            ResourceAccessFlags::Attribute |
            ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform |
            ResourceAccessFlags::Readback),
            "Image read has inappropriate flags")
        ASSERT(
            enumHasAny(readFlags, ResourceAccessFlags::Blit | ResourceAccessFlags::Copy) ||
            enumHasAny(readFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
            enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
            "Image read does not have appropriate flags")
        
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageUsage Usage{ImageUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadStorage,
                    .Usage = ImageUsage::Storage}
            },
            {
                ResourceAccessFlags::Sampled,
                ResourceSubAccess{
                    .Access = PipelineAccess::ReadSampled,
                    .Usage = ImageUsage::Sampled}
            },
            {
                ResourceAccessFlags::Blit,
                ResourceSubAccess{
                    .Stage = PipelineStage::Blit,
                    .Access = PipelineAccess::ReadTransfer,
                    .Usage = ImageUsage::Source}
            },
            {
                ResourceAccessFlags::Copy,
                ResourceSubAccess{
                    .Stage = PipelineStage::Copy,
                    .Access = PipelineAccess::ReadTransfer,
                    .Usage = ImageUsage::Source}
            },
        };
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;

        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(readFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }

        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceWriteAccess(BufferDescription& description,
        ResourceAccessFlags writeFlags)
    {
        ASSERT(!enumHasAny(writeFlags,
           ResourceAccessFlags::Blit | ResourceAccessFlags::Sampled), "Buffer write has inappropriate flags")
        ASSERT(!enumHasAny(writeFlags, ResourceAccessFlags::Readback), "Cannot use readback flag in write operation")
        
        struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            BufferUsage Usage{BufferUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Index,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Index}
            },
            {
                ResourceAccessFlags::Indirect,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Indirect}
            },
            {
                ResourceAccessFlags::Conditional,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Conditional}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = BufferUsage::Storage}
            },
            {
                ResourceAccessFlags::Copy,
                ResourceSubAccess{
                    .Stage = PipelineStage::Copy,
                    .Access = PipelineAccess::WriteTransfer,
                    .Usage = BufferUsage::Destination}
            },
        }; 
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;
        
        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(writeFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }
        
        return std::make_pair(stage, access);
    }

    std::pair<PipelineStage, PipelineAccess> Graph::InferResourceWriteAccess(TextureDescription& description,
        ResourceAccessFlags writeFlags)
    {
        ASSERT(!enumHasAny(writeFlags, ResourceAccessFlags::Readback), "Cannot use readback flag in write operation")
        ASSERT(!enumHasAny(writeFlags,
           ResourceAccessFlags::Attribute |
           ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
           ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform),
           "Image write has inappropriate flags")
       ASSERT(
           enumHasAny(writeFlags, ResourceAccessFlags::Blit | ResourceAccessFlags::Copy) ||
           enumHasAny(writeFlags, ResourceAccessFlags::Sampled | ResourceAccessFlags::Storage) ||
           enumHasAny(description.Usage, ImageUsage::Sampled | ImageUsage::Storage),
           "Image write does not have appropriate flags")
        
       struct ResourceSubAccess
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageUsage Usage{ImageUsage::None};
        };
        static const std::vector<std::pair<ResourceAccessFlags, ResourceSubAccess>> FLAGS_TO_SUB_ACCESS_MAP = {
            {
                ResourceAccessFlags::Vertex,
                ResourceSubAccess{
                    .Stage = PipelineStage::VertexShader}
            },
            {
                ResourceAccessFlags::Pixel,
                ResourceSubAccess{
                    .Stage = PipelineStage::PixelShader}
            },
            {
                ResourceAccessFlags::Compute,
                ResourceSubAccess{
                    .Stage = PipelineStage::ComputeShader}
            },
            {
                ResourceAccessFlags::Storage,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = ImageUsage::Storage}
            },
            {
                ResourceAccessFlags::Sampled,
                ResourceSubAccess{
                    .Access = PipelineAccess::WriteShader,
                    .Usage = ImageUsage::Sampled}
            },
            {
                ResourceAccessFlags::Blit,
                ResourceSubAccess{
                    .Stage = PipelineStage::Blit,
                    .Access = PipelineAccess::WriteTransfer,
                    .Usage = ImageUsage::Destination}
            },
            {
                ResourceAccessFlags::Copy,
                ResourceSubAccess{
                    .Stage = PipelineStage::Copy,
                    .Access = PipelineAccess::WriteTransfer,
                    .Usage = ImageUsage::Destination}
            },
        };
        
        PipelineStage stage = PipelineStage::None;
        PipelineAccess access = PipelineAccess::None;
        
        for (auto&& [flag, subInfo] : FLAGS_TO_SUB_ACCESS_MAP)
        {
            if (enumHasAny(writeFlags, flag))
            {
                stage |= subInfo.Stage;
                access |= subInfo.Access;
                description.Usage |= subInfo.Usage;
            }
        }

        return std::make_pair(stage, access);
    }

    ResourceTypeBase& Graph::GetResourceTypeBase(Resource resource) const
    {
        if (resource.IsTexture())
        {
            const GraphTexture& texture = m_Textures[resource.Index()];
            return (ResourceTypeBase&)texture;
        }
        
        const GraphBuffer& buffer = m_Buffers[resource.Index()];
        return (ResourceTypeBase&)buffer;
    }

    Resource Graph::AddOrCreateAccess(Resource resource, PipelineStage stage, PipelineAccess access)
    {
        ASSERT(!
           (ResourceAccess::HasWriteAccess(access) &&
            GetResourceTypeBase(resource).m_Rename.IsValid()), "Cannot write twice to the same resource")
         
        auto it = std::ranges::find_if(CurrentPass()->m_Accesses,
            [resource](auto& resourceAccess) { return resourceAccess.m_Resource == resource; });

        if (it != CurrentPass()->m_Accesses.end())
            return AddAccess(*it, stage, access);

        ResourceAccess graphResourceAccess = {};
        graphResourceAccess.m_Resource = resource;
        graphResourceAccess.m_Stage = stage;
        graphResourceAccess.m_Access = access;
        CurrentPass()->m_Accesses.push_back(graphResourceAccess);
        
        return resource;
    }

    Resource Graph::AddAccess(ResourceAccess& resource, PipelineStage stage, PipelineAccess access)
    {
        if (ResourceAccess::HasWriteAccess(access))
        {
            Resource rename = resource.m_Resource.IsBuffer() ?
                CreateResource(GetResourceTypeBase(resource.m_Resource).m_Name,
                    m_Buffers[resource.m_Resource.Index()].m_Description) :
                CreateResource(GetResourceTypeBase(resource.m_Resource).m_Name,
                    m_Textures[resource.m_Resource.Index()].m_Description);
            
            ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resource.m_Resource);
            GetResourceTypeBase(resource.m_Resource).m_Rename = rename;
            GetResourceTypeBase(resourceTypeBase.m_Rename).m_IsExternal = resourceTypeBase.m_IsExternal;
            
            ResourceAccess graphResourceAccess = {};
            graphResourceAccess.m_Resource = rename;
            graphResourceAccess.m_Stage |= stage;
            graphResourceAccess.m_Access |= access;
            CurrentPass()->m_Accesses.push_back(graphResourceAccess);

            return rename;
        }

        resource.m_Stage |= stage;
        resource.m_Access |= access;

        return resource.m_Resource;
    }

    Pass* Graph::CurrentPass() const
    {
        ASSERT(!m_CurrentPassesStack.empty(), "No active pass is set")
        
        return m_CurrentPassesStack.back();
    }

    bool Resources::IsAllocated(Resource resource) const
    {
        if (!resource.IsValid())
            return false;

        return resource.IsBuffer() ?
            m_Graph->m_Buffers[resource.Index()].m_Resource != nullptr :
            m_Graph->m_Textures[resource.Index()].m_Resource != nullptr;
    }

    const Buffer& Resources::GetBuffer(Resource resource) const
    {
        ASSERT(resource.IsBuffer(), "Provided resource handle is not a buffer")

        return *m_Graph->m_Buffers[resource.Index()].m_Resource;
    }

    const Texture& Resources::GetTexture(Resource resource) const
    {
        ASSERT(resource.IsTexture(), "Provided resource handle is not a texture")

        return *m_Graph->m_Textures[resource.Index()].m_Resource;
    }

    Texture& Resources::GetTexture(Resource resource)
    {
        return const_cast<Texture&>(const_cast<const Resources&>(*this).GetTexture(resource));
    }

    const TextureDescription& Resources::GetTextureDescription(Resource resource) const
    {
        ASSERT(resource.IsTexture(), "Provided resource handle is not a texture")

        return m_Graph->m_Textures[resource.Index()].m_Description;
    }

    std::string Graph::MermaidDump() const
    {
        auto getPassId = [](u32 passIndex)
        {
            return std::format("pass.{}", passIndex);
        };
        auto getBarrierId = [](u32 passIndex, u32 barrierIndex)
        {
            return std::format("barrier.{}.{}", passIndex, barrierIndex);
        };
        auto getSignalId = [](u32 passIndex, u32 splitBarrierIndex)
        {
            return std::format("signal_barrier.{}.{}", passIndex, splitBarrierIndex);
        };
        auto getWaitId = [](u32 passIndex, u32 splitBarrierIndex)
        {
            return std::format("wait_barrier.{}.{}", passIndex, splitBarrierIndex);
        };
        
        std::stringstream ss;
        ss << std::format("graph LR\n");

        // declare-nodes
        std::unordered_set<u32> declaredAccesses = {};
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = *m_RenderPasses[passIndex];
            std::string passName = std::format("\t{}[/Pass{}/]", getPassId(passIndex), pass.m_Name.m_Name);
            ss << std::format("{}\n", passName);

            for (auto& access : pass.m_Accesses)
            {
                if (declaredAccesses.contains(access.m_Resource.m_Value))
                    continue;
                
                std::string resourceName = GetResourceTypeBase(access.m_Resource).m_Name;
                ss << std::format("\t{}[\"{}\n", access.m_Resource.m_Value, resourceName);
                if (access.m_Resource.IsBuffer())
                {
                    const GraphBuffer* descriptionHolder = &m_Buffers[access.m_Resource.Index()];
                    while (descriptionHolder->m_Rename.IsValid())
                        descriptionHolder = &m_Buffers[descriptionHolder->m_Rename.Index()];
                    
                    const BufferDescription& description = descriptionHolder->m_Description;
                    ss << std::format("\t{}\n\t{}\"]\n", description.SizeBytes,
                        BufferUtils::bufferUsageToString(description.Usage));
                }
                else
                {
                    const GraphTexture* descriptionHolder = &m_Textures[access.m_Resource.Index()];
                    while (descriptionHolder->m_Rename.IsValid())
                        descriptionHolder = &m_Textures[descriptionHolder->m_Rename.Index()];
                    
                    const TextureDescription& description = descriptionHolder->m_Description;
                    ss << std::format("\t({} x {} x {})\n\t{}\n\t{}\n\t{}\n\t{}\"]\n",
                        description.Width, description.Height, description.Layers,
                        ImageUtils::imageKindToString(description.Kind),
                        FormatUtils::formatToString(description.Format),
                        ImageUtils::imageUsageToString(description.Usage),
                        ImageUtils::imageFilterToString(description.MipmapFilter));
                }
                
                declaredAccesses.emplace(access.m_Resource.m_Value);
            }

            for (u32 barrierIndex = 0; barrierIndex < pass.m_BarrierDependencyInfos.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                std::string barrierId = getBarrierId(passIndex, barrierIndex);
                if (barrierInfo.ExecutionDependency.has_value())
                {
                    ss << std::format("\t{}{{{{\"{}\n", barrierId, "Execution barrier");
                    auto& execution = *barrierInfo.ExecutionDependency;
                    ss << std::format("\t{} - {}\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(execution.SourceStage),
                        SynchronizationUtils::pipelineStageToString(execution.DestinationStage));
                }
                else if (barrierInfo.MemoryDependency.has_value())
                {
                    ss << std::format("\t{}{{{{\"{}\n", barrierId, "Memory barrier");
                    auto& memory = *barrierInfo.MemoryDependency;
                    ss << std::format("\t{} - {}\n\t{} - {}\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(memory.SourceStage),
                        SynchronizationUtils::pipelineStageToString(memory.DestinationStage),
                        SynchronizationUtils::pipelineAccessToString(memory.SourceAccess),
                        SynchronizationUtils::pipelineAccessToString(memory.DestinationAccess));
                }
                else
                {
                    ss << std::format("\t{}{{{{\"{}\n", barrierId, "Layout transition barrier");
                    auto& transition = *barrierInfo.LayoutTransition;
                    ss << std::format("\t{} - {}\"}}}}\n",
                        ImageUtils::imageLayoutToString(transition.OldLayout),
                        ImageUtils::imageLayoutToString(transition.NewLayout));
                }
            }
            for (u32 signalIndex = 0; signalIndex < pass.m_SplitBarrierSignalInfos.size(); signalIndex++)
            {
                auto& signalInfo = pass.m_SplitBarrierSignalInfos[signalIndex];
                std::string signalId = getSignalId(passIndex, signalIndex);
                if (signalInfo.ExecutionDependency.has_value())
                {
                    ss << std::format("\t{}[/\"{}\n", signalId, "Signal execution");
                    auto& execution = *signalInfo.ExecutionDependency;
                    ss << std::format("\t{}\"\\]\n",
                        SynchronizationUtils::pipelineStageToString(execution.SourceStage));
                }
                else if (signalInfo.MemoryDependency.has_value())
                {
                    ss << std::format("\t{}[/\"{}\n", signalId, "Signal memory");
                    auto& memory = *signalInfo.MemoryDependency;
                    ss << std::format("\t{}\n\t{}\"\\]\n",
                        SynchronizationUtils::pipelineStageToString(memory.SourceStage),
                        SynchronizationUtils::pipelineAccessToString(memory.SourceAccess));
                }
                else
                {
                    ss << std::format("\t{}[/\"{}\n", signalId, "Signal layout transition");
                    auto& transition = *signalInfo.LayoutTransition;
                    ss << std::format("\t{} - {}\"\\]\n",
                        ImageUtils::imageLayoutToString(transition.OldLayout),
                        ImageUtils::imageLayoutToString(transition.NewLayout));
                }
            }
            for (u32 waitIndex = 0; waitIndex < pass.m_SplitBarrierWaitInfos.size(); waitIndex++)
            {
                auto& waitInfo = pass.m_SplitBarrierWaitInfos[waitIndex];
                std::string waitId = getWaitId(passIndex, waitIndex);
                if (waitInfo.ExecutionDependency.has_value())
                {
                    ss << std::format("\t{}[\\\"{}\n", waitId, "Wait execution");
                    auto& execution = *waitInfo.ExecutionDependency;
                    ss << std::format("\t{}\"/]\n",
                        SynchronizationUtils::pipelineStageToString(execution.DestinationStage));
                }
                else if (waitInfo.MemoryDependency.has_value())
                {
                    ss << std::format("\t{}[\\\"{}\n", waitId, "Wait memory");
                    auto& memory = *waitInfo.MemoryDependency;
                    ss << std::format("\t{}\n\t{}\"/]\n",
                        SynchronizationUtils::pipelineStageToString(memory.DestinationStage),
                        SynchronizationUtils::pipelineAccessToString(memory.DestinationAccess));
                }
                else
                {
                    ss << std::format("\t{}[\\\"{}\n", waitId, "Wait layout transition");
                    auto& transition = *waitInfo.LayoutTransition;
                    ss << std::format("\t{} - {}\"/]\n",
                        ImageUtils::imageLayoutToString(transition.OldLayout),
                        ImageUtils::imageLayoutToString(transition.NewLayout));
                }
            }
        }
        
        struct RenameData
        {
            u32 RenameOf;
            u32 Depth;
        };
        std::unordered_map<u32, RenameData> renames = {};
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = *m_RenderPasses[passIndex];
            std::unordered_set<u32> processedResourcesRead = {};
            std::unordered_set<u32> processedResourcesWrite = {};
            for (u32 signalIndex = 0; signalIndex < pass.m_SplitBarrierSignalInfos.size(); signalIndex++)
            {
                auto& signalInfo = pass.m_SplitBarrierSignalInfos[signalIndex];
                std::string signalId = getSignalId(passIndex, signalIndex);
                ss << std::format("\t{} -- signals --> {}\n", getPassId(passIndex), signalId);
                ss << std::format("\t{} --> {}\n", signalId, signalInfo.Resource.m_Value);
            }
            for (u32 waitIndex = 0; waitIndex < pass.m_SplitBarrierWaitInfos.size(); waitIndex++)
            {
                auto& waitInfo = pass.m_SplitBarrierWaitInfos[waitIndex];
                std::string waitId = getWaitId(passIndex, waitIndex);
                ss << std::format("\t{} --> {}\n", waitId, waitInfo.Resource.m_Value);
                ss << std::format("\t{} -- waits for --> {}\n", getPassId(passIndex), waitId);
            }
            for (u32 barrierIndex = 0; barrierIndex < pass.m_BarrierDependencyInfos.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                std::string barrierId = getBarrierId(passIndex, barrierIndex);
                ResourceAccess access = *std::ranges::find_if(pass.m_Accesses,
                    [&barrierInfo](auto& access){ return access.m_Resource == barrierInfo.Resource; });
                
                if (ResourceAccess::HasWriteAccess(access.m_Access))
                {
                    ss << std::format("\t{} -- waits for --> {}\n", getPassId(passIndex), barrierId);
                    ss << std::format("\t{} -- to write to --> {}\n", barrierId, barrierInfo.Resource.m_Value);
                    processedResourcesWrite.emplace(barrierInfo.Resource.m_Value);
                }
                if (ResourceAccess::HasReadAccess(access.m_Access))
                {
                    ss << std::format("\t{} -- waited on --> {}\n", barrierInfo.Resource.m_Value, barrierId);
                    ss << std::format("\t{} -- to be read by --> {}\n", barrierId, getPassId(passIndex));
                    processedResourcesRead.emplace(barrierInfo.Resource.m_Value);
                }
            }
            
            for (auto& access : pass.m_Accesses)
            {
                Resource rename = GetResourceTypeBase(access.m_Resource).m_Rename;
                if (rename.IsValid() && !renames.contains(rename.m_Value))
                {
                    u32 renameOf = access.m_Resource.m_Value;
                    u32 depth = 1;
                    if (renames.contains(renameOf))
                    {
                        RenameData& renameData = renames.at(renameOf);
                        renameOf = renameData.RenameOf;
                        depth = renameData.Depth + 1;
                    }
                    ss << std::format("\t{} -. rename of (depth {}) .-> {}\n",
                        rename.m_Value, depth, renameOf);
                    renames.emplace(rename.m_Value, RenameData{.RenameOf = renameOf, .Depth = depth});
                }

                if (ResourceAccess::HasWriteAccess(access.m_Access) &&
                    !processedResourcesWrite.contains(access.m_Resource.m_Value))
                        ss << std::format("\t{} -- writes to --> {}\n",
                            getPassId(passIndex), access.m_Resource.m_Value);
                if (ResourceAccess::HasReadAccess(access.m_Access) &&
                    !processedResourcesRead.contains(access.m_Resource.m_Value))
                        ss << std::format("\t{} -- read by --> {}\n",
                            access.m_Resource.m_Value, getPassId(passIndex));
            }
        }

        return ss.str();
    }

    void Graph::MermaidDumpHTML(std::string_view path) const
    {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream out(path.data());

        static constexpr std::string_view templateString = R"(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>Mermaid Flowchart</title>
                <style type="text/css">
                    #mySvgId {{
                        height: 90%;
                        width: 90%;
                    }}
                </style>
            </head>

            <body>
                <div id="graphDiv"></div>
                <script src="https://bumbu.me/svg-pan-zoom/dist/svg-pan-zoom.js"></script>
                <script type="module">
                    import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.esm.min.mjs';

                    mermaid.initialize({{
                        startOnLoad: true,
                        maxTextSize: Number.MAX_SAFE_INTEGER,
                        maxEdges: Number.MAX_SAFE_INTEGER
                    }});

                    const drawDiagram = async function () {{
                        const element = document.querySelector('#graphDiv');
                        const graphDefinition = `
                            {}
                        `;
                        const {{ svg }} = await mermaid.render('mySvgId', graphDefinition);
                        element.innerHTML = svg.replace(/[ ]*max-width:[ 0-9\.]*px;/i, '');
                        var panZoomTiger = svgPanZoom('#mySvgId', {{
                            zoomEnabled: true,
                            controlIconsEnabled: true,
                            fit: true,
                            center: true
                        }})
                    }};
                    await drawDiagram();
                    document.getElementById('mySvgId').setAttribute("height", "100vh");
                    document.getElementById('mySvgId').setAttribute("width", "100vw");
                </script>
            </body>
            </html>
        )";
        
        /* the future is now */
        std::print(out, templateString, MermaidDump());
    }
}

