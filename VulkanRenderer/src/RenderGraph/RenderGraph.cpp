#include "RenderGraph.h"

#include <sstream>
#include <stack>
#include <unordered_set>

#include "Renderer.h"
#include "Rendering/Synchronization.h"
#include "Vulkan/RenderCommand.h"

namespace RenderGraph
{
    Graph::Graph()
    {
        DescriptorArenaAllocator resourceAllocator = DescriptorArenaAllocator::Builder()
                .Kind(DescriptorAllocatorKind::Resources)
                .Residence(DescriptorAllocatorResidence::CPU)
                .ForTypes({DescriptorType::UniformBuffer, DescriptorType::StorageBuffer})
                .Count(1000)
                .Build();

        DescriptorArenaAllocator samplerAllocator =  DescriptorArenaAllocator::Builder()
                .Kind(DescriptorAllocatorKind::Samplers)
                .Residence(DescriptorAllocatorResidence::CPU)
                .ForTypes({DescriptorType::Sampler})
                .Count(32)
                .Build();
        
        m_ArenaAllocators = std::make_unique<DescriptorArenaAllocators>(resourceAllocator, samplerAllocator);
    }

    Graph::~Graph()
    {
        m_FrameDeletionQueue.Flush();
    }

    Resource Graph::SetBackbuffer(const Texture& texture)
    {
        m_Backbuffer = AddExternal("graph-backbuffer", texture);

        return m_Backbuffer;
    }

    Resource Graph::GetBackbuffer() const
    {
        ASSERT(m_Backbuffer.IsValid(), "Backbuffer is not set")

        return m_Backbuffer;
    }

    Resource Graph::AddExternal(const std::string& name, const Buffer& buffer)
    {
        Resource bufferResource = CreateResource(name, buffer.GetDescription());
        GetResourceTypeBase(bufferResource).m_IsExternal = true;
        m_Buffers[bufferResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(buffer));

        return bufferResource;
    }

    Resource Graph::AddExternal(const std::string& name, const Texture& texture)
    {
        Resource textureResource = CreateResource(name, texture.GetDescription());
        GetResourceTypeBase(textureResource).m_IsExternal = true;
        m_Textures[textureResource.Index()].SetPhysicalResource(m_Pool.AddExternalResource(texture));

        return textureResource;
    }

    Resource Graph::CreateResource(const std::string& name, const GraphBufferDescription& description)
    {
        return CreateResource(name, BufferDescription{
            .SizeBytes = description.SizeBytes,
            .Usage = BufferUsage::None});
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
        ASSERT(m_ResourceTarget, "Call to 'Read' outside of 'SetupFn' of render pass")

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
        ASSERT(m_ResourceTarget, "Call to 'Write' outside of 'SetupFn' of render pass")

        std::pair<PipelineStage, PipelineAccess> stageAccess = {};
        if (resource.IsBuffer())
            stageAccess = InferResourceWriteAccess(m_Buffers[resource.Index()].m_Description, writeFlags);
        else
            stageAccess = InferResourceWriteAccess(m_Textures[resource.Index()].m_Description, writeFlags);
        auto&& [stage, access] = stageAccess;

        if (GetResourceTypeBase(resource).m_IsExternal)
            m_ResourceTarget->m_CanBeCulled = false;
        
        return AddOrCreateAccess(resource, stage, access);
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return RenderTarget(resource, onLoad, onStore, {});
    }

    Resource Graph::RenderTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, const glm::vec4& clearColor)
    {
        ASSERT(m_ResourceTarget, "Call to 'RenderTarget' outside of 'SetupFn' of render pass")

        RenderTargetAccess renderTargetAccess = {};

        renderTargetAccess.m_Resource = AddOrCreateAccess(resource,
            PipelineStage::ColorOutput, PipelineAccess::WriteColorAttachment);
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Color;
        renderTargetAccess.m_ClearColor = clearColor;
        renderTargetAccess.m_OnLoad = onLoad;
        renderTargetAccess.m_OnStore = onStore;

        m_ResourceTarget->m_RenderTargetAttachmentAccess.push_back(renderTargetAccess);
        if (GetResourceTypeBase(resource).m_IsExternal)
            m_ResourceTarget->m_CanBeCulled = false;
        
        return renderTargetAccess.m_Resource;
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore)
    {
        return DepthStencilTarget(resource, onLoad, onStore, 0.0, 0);
    }

    Resource Graph::DepthStencilTarget(Resource resource, AttachmentLoad onLoad,
        AttachmentStore onStore, f32 clearDepth, u32 clearStencil)
    {
        ASSERT(m_ResourceTarget, "Call to 'DepthStencilTarget' outside of 'SetupFn' of render pass")

        DepthStencilAccess depthStencilAccess = {};
        
        depthStencilAccess.m_Resource = AddOrCreateAccess(resource,
            PipelineStage::DepthLate, PipelineAccess::WriteDepthStencilAttachment);
        m_Textures[resource.Index()].m_Description.Usage |= ImageUsage::Depth | ImageUsage::Stencil;
        depthStencilAccess.m_ClearDepth = clearDepth;
        depthStencilAccess.m_ClearStencil = clearStencil;
        depthStencilAccess.m_OnLoad = onLoad;
        depthStencilAccess.m_OnStore = onStore;
        depthStencilAccess.m_IsDepthOnly = m_Textures[resource.Index()].m_Description.Format == Format::D32_FLOAT;

        m_ResourceTarget->m_DepthStencilAccess = depthStencilAccess;
        if (GetResourceTypeBase(resource).m_IsExternal)
            m_ResourceTarget->m_CanBeCulled = false;

        return depthStencilAccess.m_Resource;
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
        m_FrameDeletionQueue.Flush();
        for (auto& pass: m_RenderPasses)
        {
            pass->m_LayoutTransitions.clear();
            pass->m_Barriers.clear();

            pass->m_LayoutTransitionInfos.clear();
            pass->m_Barriers.clear();

            for (auto& resourceAccess : pass->m_Accesses)
            {
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resourceAccess.m_Resource);
                resourceTypeBase.m_FirstAccess = resourceTypeBase.m_LastAccess = ResourceTypeBase::NON_INDEX;
            }
        }
    }

    void Graph::Compile()
    {
        Clear();
        
        PreprocessResources();

        CullPasses();

        CalculateResourcesLifeSpan();

        CreatePhysicalResources();        

        ManageBarriers();
    }

    void Graph::Execute(FrameContext& frameContext)
    {
        RenderCommand::Bind(frameContext.Cmd, GetArenaAllocators());
        
        Resources resources = {*this};
        for (auto& pass : m_RenderPasses)
        {
            for (auto& barrier : pass->m_Barriers)
                RenderCommand::WaitOnBarrier(frameContext.Cmd, barrier);
            for (auto& layoutTransition : pass->m_LayoutTransitions)
                RenderCommand::WaitOnBarrier(frameContext.Cmd, layoutTransition);

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
                RenderCommand::SetViewport(frameContext.Cmd, resolution);
                RenderCommand::SetScissors(frameContext.Cmd, {0, 0}, resolution);

                std::vector<RenderingAttachment> attachments;
                attachments.reserve(pass->m_RenderTargetAttachmentAccess.size() +
                    (u32)pass->m_DepthStencilAccess.has_value());
                for (auto& target : pass->m_RenderTargetAttachmentAccess)
                    attachments.push_back(RenderingAttachment::Builder()
                        .ClearValue(target.m_ClearColor)
                        .SetType(RenderingAttachmentType::Color)
                        .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                        .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource, ImageLayout::ColorAttachment)
                        .Build(m_FrameDeletionQueue));
                if (pass->m_DepthStencilAccess.has_value())
                {
                    auto target = pass->m_DepthStencilAccess.value();

                    ImageLayout layout = target.m_IsDepthOnly ?
                        ImageLayout::DepthAttachment : ImageLayout::DepthStencilAttachment;
                    
                    attachments.push_back(RenderingAttachment::Builder()
                       .ClearValue(target.m_ClearDepth, target.m_ClearStencil)
                       .SetType(RenderingAttachmentType::Depth)
                       .LoadStoreOperations(target.m_OnLoad, target.m_OnStore)
                       .FromImage(*m_Textures[target.m_Resource.Index()].m_Resource, layout)
                       .Build(m_FrameDeletionQueue));
                }
                RenderingInfo::Builder renderingInfoBuilder = RenderingInfo::Builder()
                    .SetRenderArea(resolution);
                for (auto& attachment : attachments)
                    renderingInfoBuilder.AddAttachment(attachment);

                RenderingInfo renderingInfo = renderingInfoBuilder.Build(m_FrameDeletionQueue);
                
                RenderCommand::BeginRendering(frameContext.Cmd, renderingInfo);

                pass->Execute(frameContext, resources);

                RenderCommand::EndRendering(frameContext.Cmd);
            }
            else
            {
                pass->Execute(frameContext, resources);
            }
            
        }


        if (!m_Backbuffer.IsValid())
            return;
        
        // transition backbuffer to the layout that swapchain expects
        const Texture& backbuffer = *m_Textures[m_Backbuffer.Index()].m_Resource;
        ImageSubresource backbufferSubresource = backbuffer.CreateSubresource(0, 1, 0, 1);
        LayoutTransitionInfo backbufferTransition = {
            .ImageSubresource = &backbufferSubresource,
            .SourceStage = PipelineStage::AllCommands,
            .DestinationStage = PipelineStage::Bottom,
            .SourceAccess = PipelineAccess::WriteAll | PipelineAccess::ReadAll,
            .DestinationAccess = PipelineAccess::None,
            .OldLayout = m_BackbufferLayout,
            .NewLayout = ImageLayout::Source};
        DependencyInfo transitionDependency = DependencyInfo::Builder()
            .LayoutTransition(backbufferTransition)
            .Build(m_FrameDeletionQueue);
        RenderCommand::WaitOnBarrier(frameContext.Cmd, transitionDependency);
    }

    void Graph::PreprocessResources()
    {
        for (auto& buffer : m_Buffers)
        {
            // all buffers require device address
            buffer.m_Description.Usage |= BufferUsage::DeviceAddress;
            // todo: definitely not the best way
            buffer.m_Description.Usage |= BufferUsage::Destination;
        }
        
    }

    void Graph::CullPasses()
    {
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
        

        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            for (auto& access : m_RenderPasses[passIndex]->m_Accesses)
            {
                if (ResourceAccess::HasWriteAccess(access.m_Access))
                {
                    passRefCount[passIndex]++;
                    access.m_Resource.IsBuffer() ?
                        bufferRefCount[access.m_Resource.Index()].Producer = passIndex : 
                        textureRefCount[access.m_Resource.Index()].Producer = passIndex; 
                }
                else if (ResourceAccess::HasReadAccess(access.m_Access))
                {
                    access.m_Resource.IsBuffer() ?
                        ++bufferRefCount[access.m_Resource.Index()].Count : 
                        ++textureRefCount[access.m_Resource.Index()].Count;
                    access.m_Resource.IsBuffer() ?
                        producerMap[passIndex].ReadBuffers.push_back(access.m_Resource.Index()) : 
                        producerMap[passIndex].ReadTextures.push_back(access.m_Resource.Index()); 
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

        for (u32 passIndex = 0; passIndex < passRefCount.size(); passIndex++)
            if (passRefCount[passIndex] == 0)
                LOG("TO BE CULLED: {}", m_RenderPasses[passIndex]->m_Name);
    }

    // todo: use for barriers
    std::vector<std::vector<u32>> Graph::BuildAdjacencyList()
    {
        std::vector adjacency(m_RenderPasses.size(), std::vector<u32>{});
        // beautiful 
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
            for (u32 otherIndex = passIndex + 1; otherIndex < m_RenderPasses.size(); otherIndex++)
                for (auto& access : m_RenderPasses[passIndex]->m_Accesses)
                    for (auto& otherAccess : m_RenderPasses[otherIndex]->m_Accesses)
                        if (access.m_Resource == otherAccess.m_Resource &&
                            ResourceAccess::HasWriteAccess(access.m_Access) &&
                            ResourceAccess::HasReadAccess(otherAccess.m_Access))
                                adjacency[passIndex].push_back(otherIndex);

        return adjacency;
    }

    // todo: use for barriers
    std::vector<u32> Graph::CalculateLongestPath(const std::vector<std::vector<u32>>& adjacency)
    {
        std::vector<u32> longestPath(m_RenderPasses.size());
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
            for (u32 adjacentIndex : adjacency[passIndex])
                if (longestPath[adjacentIndex] < longestPath[passIndex] + 1)
                    longestPath[adjacentIndex] = longestPath[passIndex] + 1;

        return longestPath;
    }

    void Graph::CalculateResourcesLifeSpan()
    {
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
        auto handleAllocation = [](auto& collection, u32 index, bool needsAllocation, bool hasRename,
            const auto& allocFn)
        {
            auto& graphResource = collection[index];
            if (needsAllocation && graphResource.m_Resource == nullptr)
                graphResource.SetPhysicalResource(allocFn(graphResource));
            if (hasRename)
                collection[graphResource.m_Rename.Index()].SetPhysicalResource(graphResource.m_ResourceRef);
        };
        auto handleDeallocation = [](auto& collection, u32 index, bool needsDeallocation)
        {
            if (needsDeallocation)
                collection[index].ReleaseResource();
        };
        
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            for (auto& resourceAccess : m_RenderPasses[passIndex]->m_Accesses)
            {
                Resource resource = resourceAccess.m_Resource;
                
                ResourceTypeBase& resourceTypeBase = GetResourceTypeBase(resource);

                // if a resource has a 'Rename', the physical resource will be shared
                // it means, that upon allocation the acquired physical resource should be also referenced
                // in all of the renames (which is a linked list),
                // and the deallocation shall be delayed until resource is the last in the chain of renames
                bool hasRename = resourceTypeBase.m_Rename.IsValid();
                
                bool needsAllocation = resourceTypeBase.m_FirstAccess == passIndex &&
                    !resourceTypeBase.m_IsExternal;
                bool needsDeallocation = resourceTypeBase.m_LastAccess == passIndex &&
                    !resourceTypeBase.m_IsExternal && !hasRename;

                if (resource.IsBuffer())
                {
                    handleAllocation(m_Buffers, resource.Index(), needsAllocation, hasRename,
                        [this](auto& res){ return m_Pool.GetResource<Buffer>(res.m_Description); });
                    handleDeallocation(m_Buffers, resource.Index(), needsDeallocation);
                }
                else
                {
                    handleAllocation(m_Textures, resource.Index(), needsAllocation, hasRename,
                        [this](auto& res){ return m_Pool.GetResource<Texture>(res.m_Description); });
                    handleDeallocation(m_Textures, resource.Index(), needsDeallocation);
                }
            }
        }
    }

    void Graph::ManageBarriers()
    {
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

            TextureTransitionInfo Merge(const TextureTransitionInfo& other)
            {
                TextureTransitionInfo info = *this;
                info.DestinationStage = other.DestinationStage;
                info.DestinationAccess = other.DestinationAccess;
                info.NewLayout = other.NewLayout;

                return info;
            }
        };
        struct LayoutInfo
        {
            PipelineStage Stage{PipelineStage::None};
            PipelineAccess Access{PipelineAccess::None};
            ImageLayout Layout{ImageLayout::Undefined};
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
        };
        std::vector currentBufferAccess(m_Buffers.size(), AccessInfo{});
        std::vector currentTextureAccess(m_Textures.size(), AccessInfo{});

        auto inferDesiredLayout = [this](const ResourceAccess& access, const GraphTexture& texture)
        {
            if (enumHasAny(access.m_Access, PipelineAccess::ReadTransfer | PipelineAccess::WriteTransfer))
            {
                ASSERT(
                    enumHasOnly(access.m_Access, PipelineAccess::ReadTransfer) ||
                    enumHasOnly(access.m_Access, PipelineAccess::WriteTransfer),
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
            return ImageLayout::ReadOnly;
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
        auto addLayoutTransition = [this](Pass& pass, const TextureTransitionInfo& transition)
        {
            auto& texture = m_Textures[transition.Texture.Index()];
                
            ImageSubresource subresource = texture.m_Resource->CreateSubresource();
            LayoutTransitionInfo layoutTransitionInfo = {
                .ImageSubresource = &subresource,
                .SourceStage = transition.SourceStage,
                .DestinationStage = transition.DestinationStage,
                .SourceAccess = transition.SourceAccess,
                .DestinationAccess = transition.DestinationAccess,
                .OldLayout = transition.OldLayout,
                .NewLayout = transition.NewLayout};
            pass.m_LayoutTransitions.push_back(
                DependencyInfo::Builder()
                    .SetFlags(PipelineDependencyFlags::ByRegion)
                    .LayoutTransition(layoutTransitionInfo)
                    .Build(m_FrameDeletionQueue));
            pass.m_LayoutTransitionInfos.push_back(Pass::PassTextureTransitionInfo{
                .Texture = transition.Texture,
                .OldLayout = transition.OldLayout,
                .NewLayout = transition.NewLayout});
        };
        auto addExecutionDependency = [this](Pass& pass, Resource resource, const ExecutionDependencyInfo& dependency)
        {
            pass.m_Barriers.push_back(
               DependencyInfo::Builder()
                   .ExecutionDependency(dependency)
                   .Build(m_FrameDeletionQueue));
            pass.m_BarrierDependencyInfos.push_back({
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
                    .Build(m_FrameDeletionQueue));
            pass.m_BarrierDependencyInfos.push_back({
                .Resource = resource,
                .MemoryDependency = dependency});
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
                        .Type = accessType};
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
                            .NewLayout = desiredLayout});

                        updateAndPropagate(currentLayouts, resource, LayoutInfo{
                            .Stage = resourceAccess.m_Stage,
                            .Access = resourceAccess.m_Access,
                            .Layout = desiredLayout});

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
                    addMemoryDependency(*pass, resource, memoryDependencyInfo);
                }

                if (resource.IsBuffer())
                    updateAndPropagate(currentBufferAccess, resource, AccessInfo{
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType});
                else
                    updateAndPropagate(currentTextureAccess, resource, AccessInfo{
                        .Stage = resourceAccess.m_Stage,
                        .Access = resourceAccess.m_Access,
                        .Type = accessType});
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
                    addLayoutTransition(pass, transition.Merge(otherTransition));
                }
            };
            
            if (layoutTransitions.size() > 1)
            {
                std::ranges::sort(layoutTransitions, [](auto& a, auto& b) { return a.Texture < b.Texture; });
                u32 currentIndex = 0;
                
                for (u32 index = 1; index < layoutTransitions.size(); index++)
                {
                    auto& transition = layoutTransitions[currentIndex];
                    auto& otherTransition = layoutTransitions[index];
                    
                    if (otherTransition.Texture != transition.Texture)
                    {
                        if (index - currentIndex > 1)
                        {
                            mergeLayoutsOrConvertIntoBarrier(*pass, currentIndex, index);
                        }
                        else if (index == layoutTransitions.size() - 1)
                        {
                            addLayoutTransition(*pass, transition);
                            addLayoutTransition(*pass, otherTransition);
                        }
                        else
                        {
                            addLayoutTransition(*pass, transition);
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
                addLayoutTransition(*pass, transition);
            }
        }

        if (m_Backbuffer.IsValid())
            m_BackbufferLayout = currentLayouts[m_Backbuffer.Index()].Layout;
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
            ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform), "Image read has inappropriate flags")
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
        ASSERT(!enumHasAny(writeFlags,
           ResourceAccessFlags::Attribute |
           ResourceAccessFlags::Index | ResourceAccessFlags::Indirect | ResourceAccessFlags::Conditional |
           ResourceAccessFlags::Attribute | ResourceAccessFlags::Uniform), "Image write has inappropriate flags")
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

    Resource Graph::AddOrCreateAccess(Resource resource, PipelineStage stage,
        PipelineAccess access)
    {
        auto it = std::ranges::find_if(m_ResourceTarget->m_Accesses,
            [resource](auto& resourceAccess) { return resourceAccess.m_Resource == resource; });

        if (it != m_ResourceTarget->m_Accesses.end())
            return AddAccess(*it, stage, access);

        ResourceAccess GraphResourceAccess = {};
        GraphResourceAccess.m_Resource = resource;
        GraphResourceAccess.m_Stage = stage;
        GraphResourceAccess.m_Access = access;
        m_ResourceTarget->m_Accesses.push_back(GraphResourceAccess);
        
        return resource;
    }

    Resource Graph::AddAccess(ResourceAccess& resource, PipelineStage stage,
        PipelineAccess access)
    {
        ASSERT(
            !ResourceAccess::HasWriteAccess(access) ||
            !ResourceAccess::HasWriteAccess(resource.m_Access) ||
            !GetResourceTypeBase(resource.m_Resource).m_Rename.IsValid(), "Cannot write twice to the same resource")

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
            m_ResourceTarget->m_Accesses.push_back(graphResourceAccess);

            return rename;
        }

        resource.m_Stage |= stage;
        resource.m_Access |= access;

        return resource.m_Resource;
    }

    const Buffer& Resources::GetBuffer(Resource resource) const
    {
        ASSERT(resource.IsBuffer(), "Provided resource handle is not a buffer")

        return *m_Graph->m_Buffers[resource.Index()].m_Resource;
    }

    const Buffer& Resources::GetBuffer(Resource resource, const void* data, u64 sizeBytes,
        ResourceUploader& resourceUploader) const
    {
        return GetBuffer(resource, data, sizeBytes, 0, resourceUploader);
    }

    const Buffer& Resources::GetBuffer(Resource resource, const void* data, u64 sizeBytes, u64 offset,
        ResourceUploader& resourceUploader) const
    {
        ASSERT(resource.IsBuffer(), "Provided resource handle is not a buffer")

        Buffer& buffer = const_cast<Buffer&>(GetBuffer(resource));
        resourceUploader.UpdateBuffer(buffer, data, sizeBytes, offset);

        return buffer;
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

    std::string Graph::MermaidDump() const
    {
        auto getPassId = [](u32 passIndex)
        {
            return std::format("pass_{}", passIndex);
        };
        auto getTransitionId = [](u32 passIndex, u32 transitionIndex)
        {
            return std::format("transition_{}_{}", passIndex, transitionIndex);
        };
        auto getBarrierId = [](u32 passIndex, u32 barrierIndex)
        {
            return std::format("barrier_{}_{}", passIndex, barrierIndex);
        };
        
        std::stringstream ss;
        ss << std::format("graph TB\n");

        // declare-nodes
        std::unordered_set<u32> declaredAccesses = {};
        for (u32 passIndex = 0; passIndex < m_RenderPasses.size(); passIndex++)
        {
            auto& pass = *m_RenderPasses[passIndex];
            std::string passName = std::format("\t{}[/Pass_{}/]", getPassId(passIndex), pass.m_Name);
            ss << std::format("{}\n", passName);

            for (auto& access : pass.m_Accesses)
            {
                if (declaredAccesses.contains(access.m_Resource.m_Value))
                    continue;
                
                std::string resourceName = GetResourceTypeBase(access.m_Resource).m_Name;
                ss << std::format("\t{}[\"`{}\n", access.m_Resource.m_Value, resourceName);
                if (access.m_Resource.IsBuffer())
                {
                    const BufferDescription& description = m_Buffers[access.m_Resource.Index()].m_Description;
                    ss << std::format("\t{}\n\t{}`\"]\n", description.SizeBytes,
                        BufferUtils::bufferUsageToString(description.Usage));
                }
                else
                {
                    const TextureDescription& description = m_Textures[access.m_Resource.Index()].m_Description;
                    ss << std::format("\t({} x {} x {})\n\t{}\n\t{}\n\t{}\n\t{}`\"]\n",
                        description.Width, description.Height, description.Layers,
                        ImageUtils::imageKindToString(description.Kind),
                        FormatUtils::formatToString(description.Format),
                        ImageUtils::imageUsageToString(description.Usage),
                        ImageUtils::imageFilterToString(description.MipmapFilter));
                }
                
                declaredAccesses.emplace(access.m_Resource.m_Value);
            }

            for (u32 transitionIndex = 0; transitionIndex < pass.m_LayoutTransitions.size(); transitionIndex++)
            {
                auto& transitionInfo = pass.m_LayoutTransitionInfos[transitionIndex];
                std::string transitionId = getTransitionId(passIndex, transitionIndex);
                ss << std::format("\t{}{{{{\"`{}\n", transitionId, "Layout transition");
                ss << std::format("\t{} - {}`\"}}}}\n",
                    ImageUtils::imageLayoutToString(transitionInfo.OldLayout),
                    ImageUtils::imageLayoutToString(transitionInfo.NewLayout));
            }

            for (u32 barrierIndex = 0; barrierIndex < pass.m_Barriers.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                std::string barrierId = getBarrierId(passIndex, barrierIndex);
                ss << std::format("\t{}{{{{\"`{}\n", barrierId, "Barrier");
                if (barrierInfo.ExecutionDependency.has_value())
                {
                    auto& execution = barrierInfo.ExecutionDependency.value();
                    ss << std::format("\t{} - {}`\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(execution.SourceStage),
                        SynchronizationUtils::pipelineStageToString(execution.DestinationStage));
                }
                else
                {
                    auto& memory = barrierInfo.MemoryDependency.value();
                    ss << std::format("\t{} - {}\n\t{} - {}`\"}}}}\n",
                        SynchronizationUtils::pipelineStageToString(memory.SourceStage),
                        SynchronizationUtils::pipelineStageToString(memory.DestinationStage),
                        SynchronizationUtils::pipelineAccessToString(memory.SourceAccess),
                        SynchronizationUtils::pipelineAccessToString(memory.DestinationAccess));
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
            std::unordered_map<u32, u32> transitionedTextures = {};
            for (u32 transitionIndex = 0; transitionIndex < pass.m_LayoutTransitions.size(); transitionIndex++)
            {
                auto& transitionInfo = pass.m_LayoutTransitionInfos[transitionIndex];
                transitionedTextures.emplace(transitionInfo.Texture.m_Value, transitionIndex);
            }
            std::unordered_map<u32, u32> barriers = {};
            for (u32 barrierIndex = 0; barrierIndex < pass.m_Barriers.size(); barrierIndex++)
            {
                auto& barrierInfo = pass.m_BarrierDependencyInfos[barrierIndex];
                barriers.emplace(barrierInfo.Resource.m_Value, barrierIndex);
            }
            for (auto& access : pass.m_Accesses)
            {
                // resource can have rename only if it is written to
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

                
                if (ResourceAccess::HasWriteAccess(access.m_Access))
                {
                    if (transitionedTextures.contains(access.m_Resource.m_Value))
                    {
                        u32 transitionIndex = transitionedTextures.at(access.m_Resource.m_Value);
                        std::string transitionId = getTransitionId(passIndex, transitionIndex);
                        ss << std::format("\t{} -- waits for --> {}\n",
                            getPassId(passIndex), transitionId);
                        ss << std::format("\t{} -- to write to --> {}\n",
                            transitionId, access.m_Resource.m_Value);
                    }
                    else if (barriers.contains(access.m_Resource.m_Value))
                    {
                        u32 barrierIndex = barriers.at(access.m_Resource.m_Value);
                        std::string barrierId = getBarrierId(passIndex, barrierIndex);
                        ss << std::format("\t{} -- waits for --> {}\n",
                            getPassId(passIndex), barrierId);
                        ss << std::format("\t{} -- to write to --> {}\n",
                            barrierId, access.m_Resource.m_Value);
                    }
                    else
                    {
                        ss << std::format("\t{} -- writes to --> {}\n",
                            getPassId(passIndex), access.m_Resource.m_Value);
                    }
                }
                if (ResourceAccess::HasReadAccess(access.m_Access))
                {
                    if (transitionedTextures.contains(access.m_Resource.m_Value))
                    {
                        u32 transitionIndex = transitionedTextures.at(access.m_Resource.m_Value);
                        std::string transitionId = getTransitionId(passIndex, transitionIndex);
                        ss << std::format("\t{} -- transitions --> {}\n",
                            access.m_Resource.m_Value, transitionId);
                        ss << std::format("\t{} -- to be read by --> {}\n",
                            transitionId, getPassId(passIndex));
                    }
                    else if (barriers.contains(access.m_Resource.m_Value))
                    {
                        u32 barrierIndex = barriers.at(access.m_Resource.m_Value);
                        std::string barrierId = getBarrierId(passIndex, barrierIndex);
                        ss << std::format("\t{} -- waited on --> {}\n",
                            access.m_Resource.m_Value, barrierId);
                        ss << std::format("\t{} -- to be read by --> {}\n",
                            barrierId, getPassId(passIndex));
                    }
                    else
                    {
                        ss << std::format("\t{} -- read by --> {}\n",
                            access.m_Resource.m_Value, getPassId(passIndex));
                    }
                }
            }
        }

        return ss.str();
    }
}

