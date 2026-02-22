#include "rendererpch.h"
#include "ShaderAssetManager.h"

#include "FrameContext.h"
#include "Assets/AssetSystem.h"
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

    LoadShaderInfo(path, resolver);

    return true;
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

void ShaderAssetManager::LoadShaderInfo(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return;

    const StringId name = StringId::FromString(shaderLoadInfo->Name);
    m_ShaderNameToPath.emplace(name, weakly_canonical(path).generic_string());

    for (auto& variant : shaderLoadInfo->Variants)
    {
        auto bakedPath = bakers::Slang::GetBakedPath(path, StringId::FromString(variant.Name),
            *m_BakeSettings, m_Context);

        auto assetFile = m_Context.Io->ReadHeader(bakedPath);
        if (!assetFile.has_value())
            return;

        auto shaderHeader = assetlib::shader::readHeader(*assetFile);
        if (!shaderHeader.has_value())
            return;

        for (auto& include : shaderHeader->Includes)
            if (std::ranges::find(m_PathToShaders[include], name) == m_PathToShaders[include].end())
                m_PathToShaders[include].push_back(name);

        resolver.RegisterId(assetFile->Metadata.AssetId, {
            .Path = bakedPath,
            .AssetType = assetFile->Metadata.Type
        });
    }
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

    for (const StringId name : m_PathToShaders[path.generic_string()])
    {
        const std::filesystem::path& shaderPath = m_ShaderNameToPath[name];
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

namespace
{
constexpr Format formatFromAssetImageFormat(assetlib::ImageFormat format)
{
    static_assert((u32)Format::Undefined == (u32)assetlib::ImageFormat::Undefined);
    static_assert((u32)Format::RG4_UNORM_PACK8 == (u32)assetlib::ImageFormat::RG4_UNORM_PACK8);
    static_assert((u32)Format::RGBA4_UNORM_PACK16 == (u32)assetlib::ImageFormat::RGBA4_UNORM_PACK16);
    static_assert((u32)Format::BGRA4_UNORM_PACK16 == (u32)assetlib::ImageFormat::BGRA4_UNORM_PACK16);
    static_assert((u32)Format::R5G6B5_UNORM_PACK16 == (u32)assetlib::ImageFormat::R5G6B5_UNORM_PACK16);
    static_assert((u32)Format::B5G6R5_UNORM_PACK16 == (u32)assetlib::ImageFormat::B5G6R5_UNORM_PACK16);
    static_assert((u32)Format::RGB5A1_UNORM_PACK16 == (u32)assetlib::ImageFormat::RGB5A1_UNORM_PACK16);
    static_assert((u32)Format::BGR5A1_UNORM_PACK16 == (u32)assetlib::ImageFormat::BGR5A1_UNORM_PACK16);
    static_assert((u32)Format::A1RGB5_UNORM_PACK16 == (u32)assetlib::ImageFormat::A1RGB5_UNORM_PACK16);
    static_assert((u32)Format::R8_UNORM == (u32)assetlib::ImageFormat::R8_UNORM);
    static_assert((u32)Format::R8_SNORM == (u32)assetlib::ImageFormat::R8_SNORM);
    static_assert((u32)Format::R8_USCALED == (u32)assetlib::ImageFormat::R8_USCALED);
    static_assert((u32)Format::R8_SSCALED == (u32)assetlib::ImageFormat::R8_SSCALED);
    static_assert((u32)Format::R8_UINT == (u32)assetlib::ImageFormat::R8_UINT);
    static_assert((u32)Format::R8_SINT == (u32)assetlib::ImageFormat::R8_SINT);
    static_assert((u32)Format::R8_SRGB == (u32)assetlib::ImageFormat::R8_SRGB);
    static_assert((u32)Format::RG8_UNORM == (u32)assetlib::ImageFormat::RG8_UNORM);
    static_assert((u32)Format::RG8_SNORM == (u32)assetlib::ImageFormat::RG8_SNORM);
    static_assert((u32)Format::RG8_USCALED == (u32)assetlib::ImageFormat::RG8_USCALED);
    static_assert((u32)Format::RG8_SSCALED == (u32)assetlib::ImageFormat::RG8_SSCALED);
    static_assert((u32)Format::RG8_UINT == (u32)assetlib::ImageFormat::RG8_UINT);
    static_assert((u32)Format::RG8_SINT == (u32)assetlib::ImageFormat::RG8_SINT);
    static_assert((u32)Format::RG8_SRGB == (u32)assetlib::ImageFormat::RG8_SRGB);
    static_assert((u32)Format::RGB8_UNORM == (u32)assetlib::ImageFormat::RGB8_UNORM);
    static_assert((u32)Format::RGB8_SNORM == (u32)assetlib::ImageFormat::RGB8_SNORM);
    static_assert((u32)Format::RGB8_USCALED == (u32)assetlib::ImageFormat::RGB8_USCALED);
    static_assert((u32)Format::RGB8_SSCALED == (u32)assetlib::ImageFormat::RGB8_SSCALED);
    static_assert((u32)Format::RGB8_UINT == (u32)assetlib::ImageFormat::RGB8_UINT);
    static_assert((u32)Format::RGB8_SINT == (u32)assetlib::ImageFormat::RGB8_SINT);
    static_assert((u32)Format::RGB8_SRGB == (u32)assetlib::ImageFormat::RGB8_SRGB);
    static_assert((u32)Format::BGR8_UNORM == (u32)assetlib::ImageFormat::BGR8_UNORM);
    static_assert((u32)Format::BGR8_SNORM == (u32)assetlib::ImageFormat::BGR8_SNORM);
    static_assert((u32)Format::BGR8_USCALED == (u32)assetlib::ImageFormat::BGR8_USCALED);
    static_assert((u32)Format::BGR8_SSCALED == (u32)assetlib::ImageFormat::BGR8_SSCALED);
    static_assert((u32)Format::BGR8_UINT == (u32)assetlib::ImageFormat::BGR8_UINT);
    static_assert((u32)Format::BGR8_SINT == (u32)assetlib::ImageFormat::BGR8_SINT);
    static_assert((u32)Format::BGR8_SRGB == (u32)assetlib::ImageFormat::BGR8_SRGB);
    static_assert((u32)Format::RGBA8_UNORM == (u32)assetlib::ImageFormat::RGBA8_UNORM);
    static_assert((u32)Format::RGBA8_SNORM == (u32)assetlib::ImageFormat::RGBA8_SNORM);
    static_assert((u32)Format::RGBA8_USCALED == (u32)assetlib::ImageFormat::RGBA8_USCALED);
    static_assert((u32)Format::RGBA8_SSCALED == (u32)assetlib::ImageFormat::RGBA8_SSCALED);
    static_assert((u32)Format::RGBA8_UINT == (u32)assetlib::ImageFormat::RGBA8_UINT);
    static_assert((u32)Format::RGBA8_SINT == (u32)assetlib::ImageFormat::RGBA8_SINT);
    static_assert((u32)Format::RGBA8_SRGB == (u32)assetlib::ImageFormat::RGBA8_SRGB);
    static_assert((u32)Format::BGRA8_UNORM == (u32)assetlib::ImageFormat::BGRA8_UNORM);
    static_assert((u32)Format::BGRA8_SNORM == (u32)assetlib::ImageFormat::BGRA8_SNORM);
    static_assert((u32)Format::BGRA8_USCALED == (u32)assetlib::ImageFormat::BGRA8_USCALED);
    static_assert((u32)Format::BGRA8_SSCALED == (u32)assetlib::ImageFormat::BGRA8_SSCALED);
    static_assert((u32)Format::BGRA8_UINT == (u32)assetlib::ImageFormat::BGRA8_UINT);
    static_assert((u32)Format::BGRA8_SINT == (u32)assetlib::ImageFormat::BGRA8_SINT);
    static_assert((u32)Format::BGRA8_SRGB == (u32)assetlib::ImageFormat::BGRA8_SRGB);
    static_assert((u32)Format::ABGR8_UNORM_PACK32 == (u32)assetlib::ImageFormat::ABGR8_UNORM_PACK32);
    static_assert((u32)Format::ABGR8_SNORM_PACK32 == (u32)assetlib::ImageFormat::ABGR8_SNORM_PACK32);
    static_assert((u32)Format::ABGR8_USCALED_PACK32 == (u32)assetlib::ImageFormat::ABGR8_USCALED_PACK32);
    static_assert((u32)Format::ABGR8_SSCALED_PACK32 == (u32)assetlib::ImageFormat::ABGR8_SSCALED_PACK32);
    static_assert((u32)Format::ABGR8_UINT_PACK32 == (u32)assetlib::ImageFormat::ABGR8_UINT_PACK32);
    static_assert((u32)Format::ABGR8_SINT_PACK32 == (u32)assetlib::ImageFormat::ABGR8_SINT_PACK32);
    static_assert((u32)Format::ABGR8_SRGB_PACK32 == (u32)assetlib::ImageFormat::ABGR8_SRGB_PACK32);
    static_assert((u32)Format::A2RGB10_UNORM_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_UNORM_PACK32);
    static_assert((u32)Format::A2RGB10_SNORM_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_SNORM_PACK32);
    static_assert((u32)Format::A2RGB10_USCALED_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_USCALED_PACK32);
    static_assert((u32)Format::A2RGB10_SSCALED_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_SSCALED_PACK32);
    static_assert((u32)Format::A2RGB10_UINT_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_UINT_PACK32);
    static_assert((u32)Format::A2RGB10_SINT_PACK32 == (u32)assetlib::ImageFormat::A2RGB10_SINT_PACK32);
    static_assert((u32)Format::A2BGR10_UNORM_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_UNORM_PACK32);
    static_assert((u32)Format::A2BGR10_SNORM_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_SNORM_PACK32);
    static_assert((u32)Format::A2BGR10_USCALED_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_USCALED_PACK32);
    static_assert((u32)Format::A2BGR10_SSCALED_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_SSCALED_PACK32);
    static_assert((u32)Format::A2BGR10_UINT_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_UINT_PACK32);
    static_assert((u32)Format::A2BGR10_SINT_PACK32 == (u32)assetlib::ImageFormat::A2BGR10_SINT_PACK32);
    static_assert((u32)Format::R16_UNORM == (u32)assetlib::ImageFormat::R16_UNORM);
    static_assert((u32)Format::R16_SNORM == (u32)assetlib::ImageFormat::R16_SNORM);
    static_assert((u32)Format::R16_USCALED == (u32)assetlib::ImageFormat::R16_USCALED);
    static_assert((u32)Format::R16_SSCALED == (u32)assetlib::ImageFormat::R16_SSCALED);
    static_assert((u32)Format::R16_UINT == (u32)assetlib::ImageFormat::R16_UINT);
    static_assert((u32)Format::R16_SINT == (u32)assetlib::ImageFormat::R16_SINT);
    static_assert((u32)Format::R16_FLOAT == (u32)assetlib::ImageFormat::R16_FLOAT);
    static_assert((u32)Format::RG16_UNORM == (u32)assetlib::ImageFormat::RG16_UNORM);
    static_assert((u32)Format::RG16_SNORM == (u32)assetlib::ImageFormat::RG16_SNORM);
    static_assert((u32)Format::RG16_USCALED == (u32)assetlib::ImageFormat::RG16_USCALED);
    static_assert((u32)Format::RG16_SSCALED == (u32)assetlib::ImageFormat::RG16_SSCALED);
    static_assert((u32)Format::RG16_UINT == (u32)assetlib::ImageFormat::RG16_UINT);
    static_assert((u32)Format::RG16_SINT == (u32)assetlib::ImageFormat::RG16_SINT);
    static_assert((u32)Format::RG16_FLOAT == (u32)assetlib::ImageFormat::RG16_FLOAT);
    static_assert((u32)Format::RGB16_UNORM == (u32)assetlib::ImageFormat::RGB16_UNORM);
    static_assert((u32)Format::RGB16_SNORM == (u32)assetlib::ImageFormat::RGB16_SNORM);
    static_assert((u32)Format::RGB16_USCALED == (u32)assetlib::ImageFormat::RGB16_USCALED);
    static_assert((u32)Format::RGB16_SSCALED == (u32)assetlib::ImageFormat::RGB16_SSCALED);
    static_assert((u32)Format::RGB16_UINT == (u32)assetlib::ImageFormat::RGB16_UINT);
    static_assert((u32)Format::RGB16_SINT == (u32)assetlib::ImageFormat::RGB16_SINT);
    static_assert((u32)Format::RGB16_FLOAT == (u32)assetlib::ImageFormat::RGB16_FLOAT);
    static_assert((u32)Format::RGBA16_UNORM == (u32)assetlib::ImageFormat::RGBA16_UNORM);
    static_assert((u32)Format::RGBA16_SNORM == (u32)assetlib::ImageFormat::RGBA16_SNORM);
    static_assert((u32)Format::RGBA16_USCALED == (u32)assetlib::ImageFormat::RGBA16_USCALED);
    static_assert((u32)Format::RGBA16_SSCALED == (u32)assetlib::ImageFormat::RGBA16_SSCALED);
    static_assert((u32)Format::RGBA16_UINT == (u32)assetlib::ImageFormat::RGBA16_UINT);
    static_assert((u32)Format::RGBA16_SINT == (u32)assetlib::ImageFormat::RGBA16_SINT);
    static_assert((u32)Format::RGBA16_FLOAT == (u32)assetlib::ImageFormat::RGBA16_FLOAT);
    static_assert((u32)Format::R32_UINT == (u32)assetlib::ImageFormat::R32_UINT);
    static_assert((u32)Format::R32_SINT == (u32)assetlib::ImageFormat::R32_SINT);
    static_assert((u32)Format::R32_FLOAT == (u32)assetlib::ImageFormat::R32_FLOAT);
    static_assert((u32)Format::RG32_UINT == (u32)assetlib::ImageFormat::RG32_UINT);
    static_assert((u32)Format::RG32_SINT == (u32)assetlib::ImageFormat::RG32_SINT);
    static_assert((u32)Format::RG32_FLOAT == (u32)assetlib::ImageFormat::RG32_FLOAT);
    static_assert((u32)Format::RGB32_UINT == (u32)assetlib::ImageFormat::RGB32_UINT);
    static_assert((u32)Format::RGB32_SINT == (u32)assetlib::ImageFormat::RGB32_SINT);
    static_assert((u32)Format::RGB32_FLOAT == (u32)assetlib::ImageFormat::RGB32_FLOAT);
    static_assert((u32)Format::RGBA32_UINT == (u32)assetlib::ImageFormat::RGBA32_UINT);
    static_assert((u32)Format::RGBA32_SINT == (u32)assetlib::ImageFormat::RGBA32_SINT);
    static_assert((u32)Format::RGBA32_FLOAT == (u32)assetlib::ImageFormat::RGBA32_FLOAT);
    static_assert((u32)Format::R64_UINT == (u32)assetlib::ImageFormat::R64_UINT);
    static_assert((u32)Format::R64_SINT == (u32)assetlib::ImageFormat::R64_SINT);
    static_assert((u32)Format::R64_FLOAT == (u32)assetlib::ImageFormat::R64_FLOAT);
    static_assert((u32)Format::RG64_UINT == (u32)assetlib::ImageFormat::RG64_UINT);
    static_assert((u32)Format::RG64_SINT == (u32)assetlib::ImageFormat::RG64_SINT);
    static_assert((u32)Format::RG64_FLOAT == (u32)assetlib::ImageFormat::RG64_FLOAT);
    static_assert((u32)Format::RGB64_UINT == (u32)assetlib::ImageFormat::RGB64_UINT);
    static_assert((u32)Format::RGB64_SINT == (u32)assetlib::ImageFormat::RGB64_SINT);
    static_assert((u32)Format::RGB64_FLOAT == (u32)assetlib::ImageFormat::RGB64_FLOAT);
    static_assert((u32)Format::RGBA64_UINT == (u32)assetlib::ImageFormat::RGBA64_UINT);
    static_assert((u32)Format::RGBA64_SINT == (u32)assetlib::ImageFormat::RGBA64_SINT);
    static_assert((u32)Format::RGBA64_FLOAT == (u32)assetlib::ImageFormat::RGBA64_FLOAT);
    static_assert((u32)Format::B10G11R11_UFLOAT_PACK32 == (u32)assetlib::ImageFormat::B10G11R11_UFLOAT_PACK32);
    static_assert((u32)Format::E5BGR9_UFLOAT_PACK32 == (u32)assetlib::ImageFormat::E5BGR9_UFLOAT_PACK32);
    static_assert((u32)Format::D16_UNORM == (u32)assetlib::ImageFormat::D16_UNORM);
    static_assert((u32)Format::X8_D24_UNORM_PACK32 == (u32)assetlib::ImageFormat::X8_D24_UNORM_PACK32);
    static_assert((u32)Format::D32_FLOAT == (u32)assetlib::ImageFormat::D32_FLOAT);
    static_assert((u32)Format::S8_UINT == (u32)assetlib::ImageFormat::S8_UINT);
    static_assert((u32)Format::D16_UNORM_S8_UINT == (u32)assetlib::ImageFormat::D16_UNORM_S8_UINT);
    static_assert((u32)Format::D24_UNORM_S8_UINT == (u32)assetlib::ImageFormat::D24_UNORM_S8_UINT);
    static_assert((u32)Format::D32_FLOAT_S8_UINT == (u32)assetlib::ImageFormat::D32_FLOAT_S8_UINT);
    static_assert((u32)Format::BC1_RGB_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC1_RGB_UNORM_BLOCK);
    static_assert((u32)Format::BC1_RGB_SRGB_BLOCK == (u32)assetlib::ImageFormat::BC1_RGB_SRGB_BLOCK);
    static_assert((u32)Format::BC1_RGBA_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC1_RGBA_UNORM_BLOCK);
    static_assert((u32)Format::BC1_RGBA_SRGB_BLOCK == (u32)assetlib::ImageFormat::BC1_RGBA_SRGB_BLOCK);
    static_assert((u32)Format::BC2_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC2_UNORM_BLOCK);
    static_assert((u32)Format::BC2_SRGB_BLOCK == (u32)assetlib::ImageFormat::BC2_SRGB_BLOCK);
    static_assert((u32)Format::BC3_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC3_UNORM_BLOCK);
    static_assert((u32)Format::BC3_SRGB_BLOCK == (u32)assetlib::ImageFormat::BC3_SRGB_BLOCK);
    static_assert((u32)Format::BC4_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC4_UNORM_BLOCK);
    static_assert((u32)Format::BC4_SNORM_BLOCK == (u32)assetlib::ImageFormat::BC4_SNORM_BLOCK);
    static_assert((u32)Format::BC5_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC5_UNORM_BLOCK);
    static_assert((u32)Format::BC5_SNORM_BLOCK == (u32)assetlib::ImageFormat::BC5_SNORM_BLOCK);
    static_assert((u32)Format::BC6H_UFLOAT_BLOCK == (u32)assetlib::ImageFormat::BC6H_UFLOAT_BLOCK);
    static_assert((u32)Format::BC6H_FLOAT_BLOCK == (u32)assetlib::ImageFormat::BC6H_FLOAT_BLOCK);
    static_assert((u32)Format::BC7_UNORM_BLOCK == (u32)assetlib::ImageFormat::BC7_UNORM_BLOCK);
    static_assert((u32)Format::BC7_SRGB_BLOCK == (u32)assetlib::ImageFormat::BC7_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGB8_UNORM_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGB8_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGB8_SRGB_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGB8_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGB8A1_UNORM_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGB8A1_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGB8A1_SRGB_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGB8A1_SRGB_BLOCK);
    static_assert((u32)Format::ETC2_RGBA8_UNORM_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGBA8_UNORM_BLOCK);
    static_assert((u32)Format::ETC2_RGBA8_SRGB_BLOCK == (u32)assetlib::ImageFormat::ETC2_RGBA8_SRGB_BLOCK);
    static_assert((u32)Format::EAC_R11_UNORM_BLOCK == (u32)assetlib::ImageFormat::EAC_R11_UNORM_BLOCK);
    static_assert((u32)Format::EAC_R11_SNORM_BLOCK == (u32)assetlib::ImageFormat::EAC_R11_SNORM_BLOCK);
    static_assert((u32)Format::EAC_R11G11_UNORM_BLOCK == (u32)assetlib::ImageFormat::EAC_R11G11_UNORM_BLOCK);
    static_assert((u32)Format::EAC_R11G11_SNORM_BLOCK == (u32)assetlib::ImageFormat::EAC_R11G11_SNORM_BLOCK);
    static_assert((u32)Format::ASTC_4x4_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_4x4_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_4x4_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_4x4_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_5x4_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x4_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_5x4_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x4_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_5x5_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_5x5_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_6x5_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_6x5_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_6x6_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_6x6_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x5_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x5_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x6_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x6_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_8x8_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x8_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_8x8_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x8_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x5_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x5_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x5_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x5_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x6_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x6_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x6_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x6_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x8_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x8_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x8_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x8_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_10x10_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x10_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_10x10_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x10_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_12x10_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x10_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_12x10_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x10_SRGB_BLOCK);
    static_assert((u32)Format::ASTC_12x12_UNORM_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x12_UNORM_BLOCK);
    static_assert((u32)Format::ASTC_12x12_SRGB_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x12_SRGB_BLOCK);
    static_assert((u32)Format::GBGR8_422_UNORM == (u32)assetlib::ImageFormat::GBGR8_422_UNORM);
    static_assert((u32)Format::B8G8RG8_422_UNORM == (u32)assetlib::ImageFormat::B8G8RG8_422_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_420_UNORM == (u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_420_UNORM)
        ;
    static_assert((u32)Format::G8_B8R8_2PLANE_420_UNORM == (u32)assetlib::ImageFormat::G8_B8R8_2PLANE_420_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_422_UNORM == (u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_422_UNORM)
        ;
    static_assert((u32)Format::G8_B8R8_2PLANE_422_UNORM == (u32)assetlib::ImageFormat::G8_B8R8_2PLANE_422_UNORM);
    static_assert((u32)Format::G8_B8_R8_3PLANE_444_UNORM == (u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_444_UNORM)
        ;
    static_assert((u32)Format::R10X6_UNORM_PACK16 == (u32)assetlib::ImageFormat::R10X6_UNORM_PACK16);
    static_assert((u32)Format::R10X6G10X6_UNORM_2PACK16 == (u32)assetlib::ImageFormat::R10X6G10X6_UNORM_2PACK16);
    static_assert(
        (u32)Format::R10X6G10X6B10X6A10X6_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16);
    static_assert(
        (u32)Format::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)Format::R12X4_UNORM_PACK16 == (u32)assetlib::ImageFormat::R12X4_UNORM_PACK16);
    static_assert((u32)Format::R12X4G12X4_UNORM_2PACK16 == (u32)assetlib::ImageFormat::R12X4G12X4_UNORM_2PACK16);
    static_assert(
        (u32)Format::R12X4G12X4B12X4A12X4_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16);
    static_assert(
        (u32)Format::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 == (u32)
        assetlib::ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)Format::G16B16G16R16_422_UNORM == (u32)assetlib::ImageFormat::G16B16G16R16_422_UNORM);
    static_assert((u32)Format::B16G16RG16_422_UNORM == (u32)assetlib::ImageFormat::B16G16RG16_422_UNORM);
    static_assert(
        (u32)Format::G16_B16_R16_3PLANE_420_UNORM == (u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_420_UNORM);
    static_assert(
        (u32)Format::G16_B16R16_2PLANE_420_UNORM == (u32)assetlib::ImageFormat::G16_B16R16_2PLANE_420_UNORM);
    static_assert(
        (u32)Format::G16_B16_R16_3PLANE_422_UNORM == (u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_422_UNORM);
    static_assert(
        (u32)Format::G16_B16R16_2PLANE_422_UNORM == (u32)assetlib::ImageFormat::G16_B16R16_2PLANE_422_UNORM);
    static_assert(
        (u32)Format::G16_B16_R16_3PLANE_444_UNORM == (u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_444_UNORM);
    static_assert((u32)Format::G8_B8R8_2PLANE_444_UNORM == (u32)assetlib::ImageFormat::G8_B8R8_2PLANE_444_UNORM);
    static_assert(
        (u32)Format::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16);
    static_assert(
        (u32)Format::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16 == (u32)
        assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16);
    static_assert(
        (u32)Format::G16_B16R16_2PLANE_444_UNORM == (u32)assetlib::ImageFormat::G16_B16R16_2PLANE_444_UNORM);
    static_assert((u32)Format::A4RGB4_UNORM_PACK16 == (u32)assetlib::ImageFormat::A4RGB4_UNORM_PACK16);
    static_assert((u32)Format::A4B4G4R4_UNORM_PACK16 == (u32)assetlib::ImageFormat::A4B4G4R4_UNORM_PACK16);
    static_assert((u32)Format::ASTC_4x4_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_4x4_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_5x4_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x4_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_5x5_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_5x5_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_6x5_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x5_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_6x6_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_6x6_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x5_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x5_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x6_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x6_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_8x8_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_8x8_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x5_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x5_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x6_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x6_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x8_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x8_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_10x10_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_10x10_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_12x10_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x10_FLOAT_BLOCK);
    static_assert((u32)Format::ASTC_12x12_FLOAT_BLOCK == (u32)assetlib::ImageFormat::ASTC_12x12_FLOAT_BLOCK);
    static_assert((u32)Format::A1BGR5_UNORM_PACK16 == (u32)assetlib::ImageFormat::A1BGR5_UNORM_PACK16);
    static_assert((u32)Format::A8_UNORM == (u32)assetlib::ImageFormat::A8_UNORM);
    static_assert(
        (u32)Format::PVRTC1_2BPP_UNORM_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC1_4BPP_UNORM_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC2_2BPP_UNORM_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC2_4BPP_UNORM_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC1_2BPP_SRGB_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC1_4BPP_SRGB_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC2_2BPP_SRGB_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG);
    static_assert(
        (u32)Format::PVRTC2_4BPP_SRGB_BLOCK_IMG == (u32)assetlib::ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG);
    static_assert((u32)Format::R8_BOOL_ARM == (u32)assetlib::ImageFormat::R8_BOOL_ARM);
    static_assert((u32)Format::RG16_SFIXED5_NV == (u32)assetlib::ImageFormat::RG16_SFIXED5_NV);
    static_assert((u32)Format::R10X6_UINT_PACK16_ARM == (u32)assetlib::ImageFormat::R10X6_UINT_PACK16_ARM);
    static_assert(
        (u32)Format::R10X6G10X6_UINT_2PACK16_ARM == (u32)assetlib::ImageFormat::R10X6G10X6_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM == (u32)
        assetlib::ImageFormat::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM);
    static_assert((u32)Format::R12X4_UINT_PACK16_ARM == (u32)assetlib::ImageFormat::R12X4_UINT_PACK16_ARM);
    static_assert(
        (u32)Format::R12X4G12X4_UINT_2PACK16_ARM == (u32)assetlib::ImageFormat::R12X4G12X4_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM == (u32)
        assetlib::ImageFormat::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM);
    static_assert((u32)Format::R14X2_UINT_PACK16_ARM == (u32)assetlib::ImageFormat::R14X2_UINT_PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2_UINT_2PACK16_ARM == (u32)assetlib::ImageFormat::R14X2G14X2_UINT_2PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM == (u32)
        assetlib::ImageFormat::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM);
    static_assert((u32)Format::R14X2_UNORM_PACK16_ARM == (u32)assetlib::ImageFormat::R14X2_UNORM_PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2_UNORM_2PACK16_ARM == (u32)assetlib::ImageFormat::R14X2G14X2_UNORM_2PACK16_ARM);
    static_assert(
        (u32)Format::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM == (u32)
        assetlib::ImageFormat::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM);
    static_assert(
        (u32)Format::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM == (u32)
        assetlib::ImageFormat::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM);
    static_assert(
        (u32)Format::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM == (u32)
        assetlib::ImageFormat::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM);

    return (Format)format;
}

constexpr DynamicStates dynamicStatesFromAssetDynamicStates(
    const assetlib::ShaderRasterizationDynamicStates& states)
{
    static_assert((u8)DynamicStates::None == (u8)assetlib::ShaderRasterizationDynamicState::None);
    static_assert((u8)DynamicStates::Viewport == BIT((u8)assetlib::ShaderRasterizationDynamicState::Viewport));
    static_assert((u8)DynamicStates::Scissor == BIT((u8)assetlib::ShaderRasterizationDynamicState::Scissor));
    static_assert((u8)DynamicStates::DepthBias == BIT((u8)assetlib::ShaderRasterizationDynamicState::DepthBias));

    DynamicStates dynamicStates = DynamicStates::None;
    for (auto state : states)
        dynamicStates |= DynamicStates(BIT((u8)state));

    return dynamicStates;
}

constexpr AlphaBlending alphaBlendingFromAssetAlphaBlending(assetlib::ShaderRasterizationAlphaBlending blending)
{
    static_assert((u8)AlphaBlending::None == (u8)assetlib::ShaderRasterizationAlphaBlending::None);
    static_assert((u8)AlphaBlending::Over == (u8)assetlib::ShaderRasterizationAlphaBlending::Over);

    return (AlphaBlending)blending;
}

constexpr DepthMode depthModeFromAssetDepthMode(assetlib::ShaderRasterizationDepthMode depthMode)
{
    static_assert((u8)DepthMode::None == (u8)assetlib::ShaderRasterizationDepthMode::None);
    static_assert((u8)DepthMode::Read == (u8)assetlib::ShaderRasterizationDepthMode::Read);
    static_assert((u8)DepthMode::ReadWrite == (u8)assetlib::ShaderRasterizationDepthMode::ReadWrite);

    return (DepthMode)depthMode;
}

constexpr DepthTest depthTestFromAssetDepthTest(assetlib::ShaderRasterizationDepthTest depthTest)
{
    static_assert((u8)DepthTest::Equal == (u8)assetlib::ShaderRasterizationDepthTest::Equal);
    static_assert((u8)DepthTest::GreaterOrEqual == (u8)assetlib::ShaderRasterizationDepthTest::GreaterOrEqual);

    return (DepthTest)depthTest;
}

constexpr FaceCullMode faceCullModeFromAssetFaceCullMode(assetlib::ShaderRasterizationFaceCullMode faceCullMode)
{
    static_assert((u8)FaceCullMode::None == (u8)assetlib::ShaderRasterizationFaceCullMode::None);
    static_assert((u8)FaceCullMode::Back == (u8)assetlib::ShaderRasterizationFaceCullMode::Back);
    static_assert((u8)FaceCullMode::Front == (u8)assetlib::ShaderRasterizationFaceCullMode::Front);

    return (FaceCullMode)faceCullMode;
}

constexpr PrimitiveKind primitiveKindModeFromAssetPrimitiveKind(
    assetlib::ShaderRasterizationPrimitiveKind primitive)
{
    static_assert((u8)PrimitiveKind::Triangle == (u8)assetlib::ShaderRasterizationPrimitiveKind::Triangle);
    static_assert((u8)PrimitiveKind::Point == (u8)assetlib::ShaderRasterizationPrimitiveKind::Point);

    return (PrimitiveKind)primitive;
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
    const std::filesystem::path& path = m_ShaderNameToPath.at(parameters.Name);

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
    const std::filesystem::path& path = m_ShaderNameToPath.at(parameters.Name);

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
