#include "rendererpch.h"
#include "ShaderPipelineLibrary.h"

#include "FrameContext.h"
#include "ShaderAssetManager.h"
#include "Assets/AssetSystem.h"
#include "Vulkan/Device.h"

namespace lux
{
ShaderPipelineLibrary::ShaderPipelineLibrary(ShaderAssetManager& shaderAssetManager)
    : m_ShaderAssetManager(&shaderAssetManager)
{
    m_ShaderUpdatedHandler = AssetUpdatedHandler([this](const AssetUpdatedInfo& updatedShader) {
        OnShaderUpdated(updatedShader.AssetHandle);
    });
    AssetSystem::Get().SubscribeOnAssetUpdate(assetlib::shader::ASSET_TYPE, m_ShaderUpdatedHandler);
}

void ShaderPipelineLibrary::Shutdown()
{
    for (const auto& pipeline : m_PipelineCache | std::views::values | std::views::filter(
        [](Pipeline pipeline) { return pipeline.HasValue(); }))
        Device::Destroy(pipeline);
}

void ShaderPipelineLibrary::OnFrameBegin(FrameContext& ctx)
{
    const std::scoped_lock lock(m_Mutex);
    m_FrameDeletionQueue = ctx.DeletionQueue.get();
}

ShaderPipelineLibraryAllocateResult ShaderPipelineLibrary::Allocate(ShaderHandle shader,
    const ShaderOverrides& overrides, DescriptorArenaAllocators& allocators)
{
    const ShaderAsset* asset = m_ShaderAssetManager->Get(shader);
    if (!asset)
        return std::unexpected(ShaderPipelineLibraryError::FailedToAllocateDescriptors);

    auto& pipelineTemplate = asset->PipelineTemplate;
    auto& shaderReflection = pipelineTemplate.GetReflection();
    const auto setPresence = pipelineTemplate.GetSetPresence();
    
    ShaderPipeline allocatedPipeline = {};
    
    auto pipeline = FindOrCreatePipeline(shader, *asset, overrides);
    if (!pipeline)
        return std::unexpected(ShaderPipelineLibraryError::FailedToCreatePipeline);
    
    allocatedPipeline.Pipeline = *pipeline;
    allocatedPipeline.PipelineLayout = pipelineTemplate.GetPipelineLayout();
    
    for (u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        if (!setPresence[i])
            continue;

        const auto& setInfo = shaderReflection.DescriptorSetsInfo()[i];
        const bool setIsBindless = setInfo.HasBindless;
        const bool setIsTextureHeap = setIsBindless &&
            setInfo.Descriptors.size() == 1 &&
            setInfo.Descriptors.front().Type == DescriptorType::Image;
        ASSERT(!setIsBindless || setIsTextureHeap, "Shader has bindless set that is not a texture heap")

        if (setIsBindless)
            continue;

        const DescriptorsLayout descriptorsLayout = pipelineTemplate.GetDescriptorsLayout(i);

        std::optional<Descriptors> descriptors = allocators.GetTransient(i).Allocate(
            descriptorsLayout, {
                .Bindings = shaderReflection.DescriptorSetsInfo()[i].Descriptors,
                .BindlessCount = 0
            });
        if (!descriptors.has_value())
            return std::unexpected(ShaderPipelineLibraryError::FailedToAllocateDescriptors);

        allocatedPipeline.Descriptors[i] = *descriptors;
    }

    if (asset->HasTextureHeap)
        allocatedPipeline.Descriptors[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Descriptors;

    return allocatedPipeline;
}

ShaderPipelineLibraryTextureHeapResult ShaderPipelineLibrary::CreateTextureHeap(
    DescriptorArenaAllocator persistentAllocator, u32 count)
{
    std::scoped_lock lock(m_Mutex);
    if (m_TextureHeap.Descriptors.HasValue())
    {
        LUX_LOG_ERROR("Texture heap is already created");
        return std::unexpected(ShaderPipelineLibraryError::FailedToAllocateDescriptors);
    }

    const auto layout = m_ShaderAssetManager->GetTextureHeapLayout();
    const auto binding = m_ShaderAssetManager->GetTextureHeapBinding();
    
    const auto allocation = persistentAllocator.Allocate(layout, {
        .Bindings = {binding},
        .BindlessCount = count,
    });

    if (!allocation.has_value())
        return std::unexpected(ShaderPipelineLibraryError::FailedToAllocateDescriptors);

    m_TextureHeap = {
        .Descriptors = *allocation,
        .Layout = layout
    };

    std::array<DescriptorsLayout, BINDLESS_DESCRIPTORS_INDEX + 1> descriptorSets;
    std::ranges::fill(descriptorSets, Device::GetEmptyDescriptorsLayout());
    descriptorSets[BINDLESS_DESCRIPTORS_INDEX] = layout;

    const PipelineLayout pipelineLayout = Device::CreatePipelineLayout({
        .DescriptorsLayouts = descriptorSets
    });

    return ShaderTextureHeapAllocation{
        .Descriptors = *allocation,
        .PipelineLayout = pipelineLayout
    };
}

std::optional<Pipeline> ShaderPipelineLibrary::FindOrCreatePipeline(ShaderHandle shaderHandle, 
    const ShaderAsset& shader, const ShaderOverrides& overrides)
{
    CashKey key = {.Shader = shaderHandle, .Overrides = overrides};
   
    {
        const std::scoped_lock lock(m_Mutex);
        auto it = m_PipelineCache.find(key);
        if (it != m_PipelineCache.end())
            return it->second;
    }
    
    auto newPipeline = CreatePipeline(shader, overrides);
    
    {
        const std::scoped_lock lock(m_Mutex);
        auto it = m_PipelineCache.find(key);
        /* if we lost the race :( */
        if (it != m_PipelineCache.end())
        {
            Device::Destroy(newPipeline);

            return it->second;
        }
        m_PipelineCache.emplace(key, newPipeline);
    }

    return newPipeline;
}

Pipeline ShaderPipelineLibrary::CreatePipeline(const ShaderAsset& shader, const ShaderOverrides& overrides)
{
    auto& pipelineTemplate = shader.PipelineTemplate;
    auto& reflection = pipelineTemplate.GetReflection();
    PipelineStateSettings stateSettings = reflection.PipelineSettings();

    ASSERT(pipelineTemplate.GetReflection().Shaders().size() == 1)
    std::array<ShaderModule, MAX_PIPELINE_SHADER_COUNT> shaderModules{};
    std::ranges::fill(shaderModules, pipelineTemplate.GetReflection().Shaders().front());

    const auto& pipelineOverride = overrides.PipelineStateOverride;
    if (pipelineOverride.DynamicStates)
        stateSettings.DynamicStates = *pipelineOverride.DynamicStates;
    if (pipelineOverride.DepthMode)
        stateSettings.DepthMode = *pipelineOverride.DepthMode;
    if (pipelineOverride.DepthTest)
        stateSettings.DepthTest = *pipelineOverride.DepthTest;
    if (pipelineOverride.CullMode)
        stateSettings.CullMode = *pipelineOverride.CullMode;
    if (pipelineOverride.AlphaBlending)
        stateSettings.AlphaBlending = *pipelineOverride.AlphaBlending;
    if (pipelineOverride.PrimitiveKind)
        stateSettings.PrimitiveKind = *pipelineOverride.PrimitiveKind;
    if (pipelineOverride.ClampDepth)
        stateSettings.ClampDepth = *pipelineOverride.ClampDepth;
    
    ShaderDynamicSpecializations specializations = overrides.Specializations;
    const Pipeline pipeline = Device::CreatePipeline({
        .PipelineLayout = pipelineTemplate.GetPipelineLayout(),
        .Shaders = Span((const ShaderModule*)shaderModules.data(), pipelineTemplate.GetShaderStages().size()),
        .ShaderStages = pipelineTemplate.GetShaderStages(),
        .ShaderEntryPoints = pipelineTemplate.GetEntryPoints(),
        .ColorFormats = reflection.ColorFormats(),
        .DepthFormat = reflection.DepthFormat() ? *reflection.DepthFormat() : Format::Undefined,
        .StateSettings = stateSettings,
        .Specialization = specializations.ToPipelineSpecializationsView(pipelineTemplate),
        .IsComputePipeline = pipelineTemplate.IsComputeTemplate(),
    }, Device::DummyDeletionQueue());
    Device::NamePipeline(pipeline, reflection.Name().AsStringView());
    
    return pipeline;
}

void ShaderPipelineLibrary::OnShaderUpdated(ShaderHandle shader)
{
    const ShaderAsset* asset = m_ShaderAssetManager->Get(shader);
    if (!asset)
        return;

    for (auto&& [key, pipeline] : m_PipelineCache)
    {
        if (key.Shader != shader)
            continue;
        
        auto newPipeline = CreatePipeline(*asset, key.Overrides);
        
        {
            const std::scoped_lock lock(m_Mutex);
            m_FrameDeletionQueue->Enqueue(pipeline);
            pipeline = newPipeline;
        }
    }
}
}
