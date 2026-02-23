#include "rendererpch.h"
#include "ShaderAssetManager.h"

#include "FrameContext.h"
#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Bakers/Bakers.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "cvars/CVarSystem.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"
#include "Vulkan/Device.h"

namespace lux
{
bool ShaderAssetManager::AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    if (path.extension() != bakers::SHADER_ASSET_EXTENSION)
        return false;

    return LoadShaderInfo(path, resolver);
}

bool ShaderAssetManager::Bakes(const std::filesystem::path& path)
{
    return
        path.extension() == bakers::SHADER_ASSET_EXTENSION ||
        path.extension() == bakers::SHADER_ASSET_RAW_EXTENSION;
}

void ShaderAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Bakes(path))
        return OnRawFileModified(path);
}

void ShaderAssetManager::Init(const bakers::SlangBakeSettings& bakeSettings)
{
    m_Context = {
        .InitialDirectory = m_AssetSystem->GetAssetsDirectory(),
        .Io = &m_AssetSystem->GetIo(),
        .Compressor = &m_AssetSystem->GetCompressor() 
    };

    m_BakeSettings = &bakeSettings;
}

void ShaderAssetManager::Shutdown()
{
    for (const PipelineInfo& pipeline : m_Pipelines |
        std::views::filter([](const PipelineInfo& pipeline) { return pipeline.Pipeline.HasValue(); }))
        Device::Destroy(pipeline.Pipeline);

    m_ShaderPipelineTemplates.clear();
}

void ShaderAssetManager::OnFrameBegin(FrameContext& ctx)
{
    m_FrameDeletionQueue = &ctx.DeletionQueue;
}

ShaderCacheAllocateResult ShaderAssetManager::Allocate(ShaderHandle handle,
    DescriptorArenaAllocators& allocators)
{
    auto& pipelineInfo = m_Pipelines[handle.Index()];
    
    const auto setPresence = pipelineInfo.PipelineTemplate->GetSetPresence();
    for (u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        if (!setPresence[i])
            continue;

        const auto& setInfo = pipelineInfo.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i];
        const bool setIsBindless = setInfo.HasBindless;
        const bool setIsTextureHeap = setIsBindless &&
            setInfo.Descriptors.size() == 1 &&
            setInfo.Descriptors.front().Type == DescriptorType::Image;
        ASSERT(!setIsBindless || setIsTextureHeap, "Shader has bindless set that is not a texture heap")

        if (setIsBindless)
            continue;

        const DescriptorsLayout descriptorsLayout = pipelineInfo.PipelineTemplate->GetDescriptorsLayout(i);

        std::optional<Descriptors> descriptors = Device::AllocateDescriptors(
            allocators.GetTransient(i),
            descriptorsLayout, {
                .Bindings = pipelineInfo.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i].Descriptors,
                .BindlessCount = 0
            });
        if (!descriptors.has_value())
            return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);

        pipelineInfo.Descriptors[i] = *descriptors;
        pipelineInfo.DescriptorLayouts[i] = descriptorsLayout;
    }

    if (pipelineInfo.HasTextureHeap)
    {
        pipelineInfo.Descriptors[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Descriptors;
        pipelineInfo.DescriptorLayouts[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Layout;
    }

    return {};
}

ShaderCacheTextureHeapResult ShaderAssetManager::AllocateTextureHeap(DescriptorArenaAllocator persistentAllocator,
    u32 count)
{
    if (m_TextureHeap.Descriptors.HasValue())
    {
        LOG("Warning: texture heap is already added");
        return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);
    }

    const DescriptorBinding descriptorBinding = {
        .Binding = BINDLESS_DESCRIPTORS_TEXTURE_BINDING_INDEX,
        .Type = DescriptorType::Image,
        .Count = descriptors::safeBindlessCountForDescriptorType(DescriptorType::Image),
        .Shaders = ShaderStage::Vertex | ShaderStage::Pixel | ShaderStage::Compute,
        .Flags = descriptors::BINDLESS_DESCRIPTORS_FLAGS,
    };
    const DescriptorsLayout layout = Device::CreateDescriptorsLayout({
        .Bindings = {descriptorBinding},
        .Flags = descriptors::BINDLESS_DESCRIPTORS_LAYOUT_FLAGS
    });

    const auto allocation = Device::AllocateDescriptors(persistentAllocator, layout, {
        .Bindings = {descriptorBinding},
        .BindlessCount = count,
    });

    if (!allocation.has_value())
        return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);

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

ShaderHandle ShaderAssetManager::LoadAsset(const ShaderLoadParameters& parameters)
{
    const ShaderNameWithOverrides nameWithOverrides = {
        .Name = parameters.Name,
        .Variant = parameters.Variant.value_or(bakers::Slang::MAIN_VARIANT),
        .OverridesHash = parameters.Overrides->Hash
    };
    auto it = m_PipelinesMap.find(nameWithOverrides);
    if (it != m_PipelinesMap.end() && !m_Pipelines[it->second.Index()].ShouldReload)
        return it->second;

    std::optional<PipelineInfo> newPipeline = TryCreatePipeline(parameters, nameWithOverrides);
    if (!newPipeline.has_value())
        return {};

    ShaderHandle handle;
    if (it == m_PipelinesMap.end())
    {
        handle = ShaderHandle((u32)m_Pipelines.size(), 0);
        m_Pipelines.push_back(*newPipeline);
        m_PipelinesMap[nameWithOverrides] = handle;
    }
    else
    {
        handle = it->second;
        m_FrameDeletionQueue->Enqueue(m_Pipelines[handle.Index()].Pipeline);
        m_Pipelines[handle.Index()] = *newPipeline;
    }

    return handle;
}

void ShaderAssetManager::UnloadAsset(ShaderHandle)
{
    ASSERT(false, "Unload is not supported for shader assets")
}

ShaderAssetManager::GetType ShaderAssetManager::GetAsset(ShaderHandle handle) const
{
    const auto& pipelineInfo = m_Pipelines[handle.Index()];
    
    Shader shader = {};
    shader.m_Pipeline = pipelineInfo.Pipeline;
    shader.m_PipelineLayout = pipelineInfo.Layout;
    shader.m_Descriptors = pipelineInfo.Descriptors;
    shader.m_DescriptorLayouts = pipelineInfo.DescriptorLayouts;
    
    return shader;
}

bool ShaderAssetManager::LoadShaderInfo(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return false;

    const StringId name = StringId::FromString(shaderLoadInfo->Name);
    m_ShaderNameToBakedPath.emplace(name, weakly_canonical(path).generic_string());

    for (auto& variant : shaderLoadInfo->Variants)
    {
        auto bakedPath = bakers::Slang::GetBakedPath(path, StringId::FromString(variant.Name),
            *m_BakeSettings, m_Context);

        auto assetFile = m_Context.Io->ReadHeader(bakedPath);
        if (!assetFile.has_value())
            return false;

        auto shaderHeader = assetlib::shader::readHeader(*assetFile);
        if (!shaderHeader.has_value())
            return false;

        for (auto& include : shaderHeader->Includes)
            if (std::ranges::find(m_RawPathToShaders[include], name) == m_RawPathToShaders[include].end())
                m_RawPathToShaders[include].push_back(name);

        resolver.RegisterId(assetFile->Metadata.AssetId, {
            .Path = bakedPath,
            .AssetType = assetFile->Metadata.Type
        });
    }

    return true;
}

void ShaderAssetManager::OnBakedFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);
    
    auto it = m_BakedPathToShaderName.find(path.string());
    if (it == m_BakedPathToShaderName.end())
        return;

    const ShaderNameWithOverrides& name = it->second;
    auto pipelineIt = m_PipelinesMap.find(name);
    if (pipelineIt == m_PipelinesMap.end())
        return;

    m_Pipelines[pipelineIt->second.Index()].ShouldReload = true;
}

void ShaderAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);

    for (const StringId name : m_RawPathToShaders[path.generic_string()])
    {
        const std::filesystem::path& shaderPath = m_ShaderNameToBakedPath[name];
        if (!m_RawPathToRebakeInfos.contains(shaderPath.string()))
            continue;

        for (auto& rebakeInfo : m_RawPathToRebakeInfos[shaderPath.string()])
        {
            m_AssetSystem->AddBakeRequest({.BakeFn = [this, shaderPath, &rebakeInfo]() {
                bakers::Slang baker;
                
                auto bakedPath = baker.BakeToFile(shaderPath, rebakeInfo.Variant, {
                        .Defines = rebakeInfo.Defines,
                        .DefinesHash = rebakeInfo.DefinesHash,
                        .IncludePaths = m_BakeSettings->IncludePaths,
                        .UniformReflectionDirectoryName = m_BakeSettings->UniformReflectionDirectoryName,
                        .EnableHotReloading = m_BakeSettings->EnableHotReloading
                    },
                    m_Context);
                if (!bakedPath)
                {
                    LOG("Warning: Bake request failed {} ({})", bakedPath.error(), shaderPath.string());
                    return;
                }
                
                OnBakedFileModified(*bakedPath);
            }});
        }
    }
}

Result<std::filesystem::path, AssetManager::IoError> ShaderAssetManager::Bake(const ShaderLoadParameters& parameters,
    const ShaderNameWithOverrides& name)
{
    auto& overrides = *parameters.Overrides;
    std::vector<std::pair<std::string, std::string>> defines;
    defines.reserve(overrides.Defines.Defines.size());
    for (auto& define : overrides.Defines.Defines)
        defines.emplace_back(define.Name.AsStringView(), define.Value);

    bakers::Slang baker;
    bakers::SlangBakeSettings settings = {
        .Defines = defines,
        .DefinesHash = overrides.Defines.Hash,
        .IncludePaths = m_BakeSettings->IncludePaths,
        .EnableHotReloading = (bool)CVars::Get().GetI32CVar("Renderer.Shaders.HotReload"_hsv).value_or(true)
    };
    const std::filesystem::path& path = m_ShaderNameToBakedPath.at(parameters.Name);

    StringId variant = parameters.Variant.value_or(bakers::Slang::MAIN_VARIANT);
    
    auto bakedPath = baker.BakeToFile(path, variant, settings, m_Context); 
    if (!bakedPath.has_value())
        return bakedPath;

    m_BakedPathToShaderName[bakedPath->string()] = name;
    RebakeInfo rebakeInfo = {
        .Variant = variant,
        .Defines = std::vector(settings.Defines.begin(), settings.Defines.end()),
        .DefinesHash = settings.DefinesHash
    };
    auto& pathRebakes = m_RawPathToRebakeInfos[path.string()];
    if (std::ranges::find(pathRebakes, rebakeInfo) == pathRebakes.end())
        pathRebakes.push_back(std::move(rebakeInfo));
    
    return bakedPath;
}


std::optional<ShaderAssetManager::PipelineInfo> ShaderAssetManager::TryCreatePipeline(
    const ShaderLoadParameters& parameters, const ShaderNameWithOverrides& name)
{
    const std::filesystem::path& path = m_ShaderNameToBakedPath.at(parameters.Name);

    const auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return std::nullopt;

    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> descriptorsLayoutsOverrides{};
    descriptorsLayoutsOverrides[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Layout;

    const ShaderPipelineTemplate* shaderTemplate =
        GetShaderPipelineTemplate(parameters, name, descriptorsLayoutsOverrides);
    if (shaderTemplate == nullptr)
        return std::nullopt;

    auto createPipeline = [&]() -> Pipeline
    {
        std::vector<Format> colorFormats;
        std::optional<Format> depthFormat;
        DynamicStates dynamicStates = DynamicStates::Default;
        AlphaBlending alphaBlending = AlphaBlending::Over;
        DepthMode depthMode = DepthMode::ReadWrite;
        DepthTest depthTest = DepthTest::GreaterOrEqual;
        FaceCullMode cullMode = FaceCullMode::Back;
        PrimitiveKind primitiveKind = PrimitiveKind::Triangle;
        bool clampDepth = false;

        if (shaderLoadInfo->RasterizationInfo.has_value())
        {
            const auto& rasterization = *shaderLoadInfo->RasterizationInfo;
            dynamicStates = dynamicStatesFromAssetDynamicStates(rasterization.DynamicStates);
            alphaBlending = alphaBlendingFromAssetAlphaBlending(rasterization.AlphaBlending);
            depthMode = depthModeFromAssetDepthMode(rasterization.DepthMode);
            depthTest = depthTestFromAssetDepthTest(rasterization.DepthTest);
            cullMode = faceCullModeFromAssetFaceCullMode(rasterization.FaceCullMode);
            primitiveKind = primitiveKindModeFromAssetPrimitiveKind(rasterization.PrimitiveKind);
            clampDepth = rasterization.ClampDepth;

            colorFormats.reserve(rasterization.Colors.size());
            for (auto& color : rasterization.Colors)
                colorFormats.push_back(formatFromAssetImageFormat(color.Format));
            if (rasterization.Depth.has_value())
                depthFormat = formatFromAssetImageFormat(*rasterization.Depth);
        }

        ASSERT(shaderTemplate->GetReflection().Shaders().size() == 1)
        std::array<ShaderModule, MAX_PIPELINE_SHADER_COUNT> shaderModules{};
        std::ranges::fill(shaderModules, shaderTemplate->GetReflection().Shaders().front());

        auto& overrides = *parameters.Overrides;
        const Pipeline pipeline = Device::CreatePipeline({
            .PipelineLayout = shaderTemplate->GetPipelineLayout(),
            .Shaders = Span(shaderModules.data(), shaderTemplate->GetShaderStages().size()),
            .ShaderStages = shaderTemplate->GetShaderStages(),
            .ShaderEntryPoints = shaderTemplate->GetEntryPoints(),
            .ColorFormats = colorFormats,
            .DepthFormat = depthFormat ? *depthFormat : Format::Undefined,
            .DynamicStates = overrides.PipelineOverrides.DynamicStates.value_or(dynamicStates),
            .DepthMode = overrides.PipelineOverrides.DepthMode.value_or(depthMode),
            .DepthTest = overrides.PipelineOverrides.DepthTest.value_or(depthTest),
            .CullMode = overrides.PipelineOverrides.CullMode.value_or(cullMode),
            .AlphaBlending = overrides.PipelineOverrides.AlphaBlending.value_or(alphaBlending),
            .PrimitiveKind = overrides.PipelineOverrides.PrimitiveKind.value_or(primitiveKind),
            .Specialization = overrides.Specializations.ToPipelineSpecializationsView(*shaderTemplate),
            .IsComputePipeline = shaderTemplate->IsComputeTemplate(),
            .ClampDepth = overrides.PipelineOverrides.ClampDepth.value_or(clampDepth)
        }, Device::DummyDeletionQueue());
        Device::NamePipeline(pipeline, parameters.Name.AsStringView());

        return pipeline;
    };

    PipelineInfo pipelineInfo = {};
    pipelineInfo.PipelineTemplate = shaderTemplate;
    pipelineInfo.Layout = shaderTemplate->GetPipelineLayout();
    pipelineInfo.HasTextureHeap = shaderTemplate->GetSetPresence()[BINDLESS_DESCRIPTORS_INDEX];
    pipelineInfo.Pipeline = createPipeline();

    return pipelineInfo;
}

const ShaderPipelineTemplate* ShaderAssetManager::GetShaderPipelineTemplate(const ShaderLoadParameters& parameters,
    const ShaderNameWithOverrides& name,
    const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides)
{
    auto bakedPath = Bake(parameters, name);
    if (!bakedPath)
        return nullptr;

    auto shaderReflectionResult = ShaderReflection::Reflect(*bakedPath, *m_Context.Io, *m_Context.Compressor);
    if (!shaderReflectionResult.has_value())
        return nullptr;

    ShaderReflection::EntryPointsInfo entryPointsInfo = shaderReflectionResult->GetEntryPointsInfo();

    return &(m_ShaderPipelineTemplates[name] = ShaderPipelineTemplate({
        .ShaderReflection = std::move(*shaderReflectionResult),
        .DescriptorLayoutOverrides = descriptorLayoutOverrides
    }));
}
}
