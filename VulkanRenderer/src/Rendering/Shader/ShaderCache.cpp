#include "rendererpch.h"

#include "ShaderCache.h"

#include "AssetManager.h"
#include "core.h"
#include "Converters.h"
#include "Bakers/BakersUtils.h"
#include "Bakers/Shaders/SlangBaker.h"
#include "cvars/CVarSystem.h"
#include "v2/Shaders/ShaderLoadInfo.h"
#include "Vulkan/Device.h"

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

void ShaderCache::Init(bakers::Context& bakersCtx, const bakers::SlangBakeSettings& bakeSettings)
{
    m_BakersCtx = &bakersCtx;
    m_BakeSettings = &bakeSettings;
    
    InitFileWatcher();
    LoadShaderInfos();
}

void ShaderCache::Shutdown()
{
    for (const PipelineInfo& pipeline : m_Pipelines | std::views::values |
        std::views::filter([](const PipelineInfo& pipeline){ return pipeline.Pipeline.HasValue(); }))
        Device::Destroy(pipeline.Pipeline);

    if (const auto res = m_FileWatcher.StopWatching(); !res.has_value())
        LOG("Failed to stop file watcher for directory {}. Error: {}",
            *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), FileWatcher::ErrorDescription(res.error()));
}

void ShaderCache::OnFrameBegin(FrameContext& ctx)
{
    m_FrameDeletionQueue = &ctx.DeletionQueue;
    HandleModifications();
}

ShaderCacheAllocateResult ShaderCache::Allocate(StringId name, DescriptorArenaAllocators& allocators,
    ShaderCacheAllocationType allocationType)
{
    return Allocate(name, {}, allocators, allocationType);
}

ShaderCacheAllocateResult ShaderCache::Allocate(StringId name, ShaderOverridesView&& overrides,
    DescriptorArenaAllocators& allocators, ShaderCacheAllocationType allocationType)
{
    const ShaderNameWithOverrides nameWithOverrides{.Name = name, .OverridesHash = overrides.Hash};

    Shader shader = {};

    const bool hasPipeline = m_Pipelines.contains(nameWithOverrides);
    if (!hasPipeline || m_Pipelines.at(nameWithOverrides).ShouldReload)
    {
        const std::optional<PipelineInfo> pipelineInfo = TryCreatePipeline(
            nameWithOverrides, overrides, allocationType);
        if (pipelineInfo.has_value())
        {
            if (hasPipeline)
                m_FrameDeletionQueue->Enqueue(m_Pipelines.at(nameWithOverrides).Pipeline);
            m_Pipelines[nameWithOverrides] = pipelineInfo.value();
        }
        else if (!hasPipeline)
        {
            return std::unexpected(ShaderCacheError::FailedToCreatePipeline);
        }
    }
    if (!hasPipeline)
        m_ShaderNameToAllOverrides[name].push_back(nameWithOverrides);

    const PipelineInfo& pipeline = m_Pipelines.at(nameWithOverrides);
    shader.m_Pipeline = pipeline.Pipeline;
    shader.m_PipelineLayout = pipeline.Layout;

    const auto setPresence = pipeline.PipelineTemplate->GetSetPresence();
    const bool referencesOtherBindlessSet = pipeline.BindlessName != StringId{};
     for (u32 i = 0; i <= BINDLESS_DESCRIPTORS_INDEX; i++)
    {
        if (!setPresence[i])
            continue;

        if (i == BINDLESS_DESCRIPTORS_INDEX && referencesOtherBindlessSet)
            continue;

        const DescriptorsLayout descriptorsLayout = pipeline.PipelineTemplate->GetDescriptorsLayout(i);
        
        std::optional<Descriptors> descriptors = Device::AllocateDescriptors(
            allocators.Get(i),
            descriptorsLayout, {
                .Bindings = pipeline.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i].Descriptors,
                .BindlessCount = pipeline.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i].HasBindless ?
                    pipeline.BindlessCount : 0
            });
        if (!descriptors.has_value())
            return std::unexpected(ShaderCacheError::FailedToAllocateDescriptors);

        shader.m_Descriptors[i] = *descriptors;
        shader.m_DescriptorLayouts[i] = descriptorsLayout;
    }

    if (referencesOtherBindlessSet)
    {
        const DescriptorsWithLayout& referencedDescriptors = m_PersistentDescriptors.at(pipeline.BindlessName);
        shader.m_Descriptors[BINDLESS_DESCRIPTORS_INDEX] = referencedDescriptors.Descriptors;
        shader.m_DescriptorLayouts[BINDLESS_DESCRIPTORS_INDEX] = referencedDescriptors.Layout;
    }
    
    return shader;
}

void ShaderCache::AddPersistentDescriptors(StringId name, Descriptors descriptors, DescriptorsLayout descriptorsLayout)
{
    if (m_PersistentDescriptors.contains(name))
        LOG("Warning: persistent descriptors with name '{}' were already added", name);
    m_PersistentDescriptors[name] = {
        .Descriptors = descriptors,
        .Layout = descriptorsLayout
    };
}

void ShaderCache::InitFileWatcher()
{
    if (const auto res = m_FileWatcher.Watch(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv)); !res.has_value())
        LOG("Failed to init file watcher for directory {}. Error: {}",
            *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), FileWatcher::ErrorDescription(res.error()));

    m_FileWatcherHandler = ::FileWatcherHandler([this](const FileWatcherEvent& event)
    {
        const std::filesystem::path filePath = event.Name;

        /* is it possible that file is deleted or renamed before we begin to process it */
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath))
            return;

        std::lock_guard lock(m_FileUpdateMutex);
        if (event.Action == FileWatcherEvent::ActionType::Modify)
            m_ShadersToReload.emplace_back(filePath);
    });
    
    if (const auto res = m_FileWatcher.Subscribe(m_FileWatcherHandler); !res.has_value())
        LOG("Failed to subscribe to file watcher events for directory {}. Error: {}",
            *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), FileWatcher::ErrorDescription(res.error()));
}

void ShaderCache::LoadShaderInfos()
{
    for (auto& file : fs::recursive_directory_iterator(
        *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv)))
    {
        if (file.is_directory())
            continue;

        auto& path = file.path();

        if (path.extension() == SHADER_EXTENSION_SLANG)
        {
            LoadSlangShaderInfo(path);
            continue;
        }

        //todo: everything below me should go without a trace

        if (path.extension() != SHADER_EXTENSION)
            continue;

        const nlohmann::json shader = nlohmann::json::parse(std::ifstream{path});
        const StringId name = StringId::FromString(shader.at("name"));
        m_ShaderNameToPath.emplace(name, fs::weakly_canonical(path).generic_string());

        for (auto& stage : shader.at("shader_stages"))
        {
            const std::string& stagePath = fs::weakly_canonical(
                *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv) + std::string(stage)).generic_string();
            assetLib::File shaderFile;
            assetLib::loadAssetFile(stagePath, shaderFile);
            const assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);
            for (auto& include : shaderInfo.IncludedFiles)
                m_PathToShaders[include].push_back(name);
            m_PathToShaders[stagePath].push_back(name);
            m_PathToShaders[shaderInfo.OriginalFile].push_back(name);
        }
    }
}

// todo: rename once ready
void ShaderCache::LoadSlangShaderInfo(const std::filesystem::path& path)
{
    auto bakedPath = bakers::Slang::GetBakedPath(path, *m_BakeSettings, *m_BakersCtx);

    auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return;
    
    const StringId name = StringId::FromString(shaderLoadInfo->Name);
    m_ShaderNameToPath.emplace(name, fs::weakly_canonical(path).generic_string());

    auto assetFile = assetlib::io::loadAssetFileHeader(bakedPath);
    if (!assetFile.has_value())
        return;

    auto shaderHeader = assetlib::shader::unpackHeader(*assetFile);
    if (!shaderHeader.has_value())
        return;

    for (auto& include : shaderHeader->Includes)
        m_PathToShaders[include].push_back(name);
}

void ShaderCache::HandleModifications()
{
    for (auto& file : m_ShadersToReload)
    {
        const std::string extension = file.extension().string();
        if (extension == SHADER_EXTENSION)
            HandleShaderModification(file);
        else if (ShaderStageConverter::WatchesExtension(extension))
            HandleStageModification(file);
        else if (extension == SHADER_HEADER_EXTENSION)
            HandleHeaderModification(file);
    }
    m_ShadersToReload.clear();
}

void ShaderCache::HandleShaderModification(const std::filesystem::path& path)
{
    const nlohmann::json shader = nlohmann::json::parse(std::ifstream{path});
    MarkOverridesToReload(StringId::FromString(shader.at("name")));
}

void ShaderCache::HandleStageModification(const std::filesystem::path& path)
{
    for (const StringId name : m_PathToShaders[path.generic_string()])
        MarkOverridesToReload(name);
}

void ShaderCache::HandleHeaderModification(const std::filesystem::path& path)
{
    for (const StringId name : m_PathToShaders[path.generic_string()])
        MarkOverridesToReload(name);
}

void ShaderCache::MarkOverridesToReload(StringId name)
{
    const auto overrides = m_ShaderNameToAllOverrides.find(name);
    if (overrides == m_ShaderNameToAllOverrides.end())
        return;

    for (auto& override : overrides->second)
        m_Pipelines.at(override).ShouldReload = true;
}

std::optional<ShaderCache::PipelineInfo> ShaderCache::TryCreatePipeline(const ShaderNameWithOverrides& name,
    ShaderOverridesView& overrides, ShaderCacheAllocationType allocationHint)
{
    const std::string& path = m_ShaderNameToPath.at(name.Name);
    std::ifstream in(path);
    nlohmann::json json = nlohmann::json::parse(std::ifstream{path});

    std::vector<std::string> stages;
    stages.reserve(json["shader_stages"].size());
    for (auto& stage : json["shader_stages"])
        stages.push_back(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv) + std::string{stage});

    StringId referencedBindlessSet = {};
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> descriptorsLayoutsOverrides{};
    if (json.contains("bindless"))
    {
        referencedBindlessSet = StringId::FromString(json.at("bindless").get<std::string>());
        if (!m_PersistentDescriptors.contains(referencedBindlessSet))
            return std::nullopt;
        
        descriptorsLayoutsOverrides[BINDLESS_DESCRIPTORS_INDEX] =
            m_PersistentDescriptors.at(referencedBindlessSet).Layout;
    }
    
    const ShaderPipelineTemplate* shaderTemplate =
        GetShaderPipelineTemplate(name, overrides, stages, descriptorsLayoutsOverrides);
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
        
        for (auto& dynamicState : json["dynamic_states"])
        {
            static const std::unordered_map<std::string, DynamicStates> NAME_TO_STATE_MAP = {
                std::make_pair("viewport", DynamicStates::Viewport),
                std::make_pair("scissor", DynamicStates::Scissor),
                std::make_pair("depth_bias", DynamicStates::DepthBias),
                std::make_pair("default", DynamicStates::Default),
            };

            auto it = NAME_TO_STATE_MAP.find(dynamicState);
            if (it == NAME_TO_STATE_MAP.end())
            {
                LOG("Unrecognized dynamic state: {}", std::string{dynamicState});
                continue;
            }
            dynamicStates |= it->second;
        }

        if (!shaderTemplate->IsComputeTemplate())
        {
            static const std::unordered_map<std::string, AlphaBlending> NAME_TO_BLENDING_MAP = {
                std::make_pair("none", AlphaBlending::None),
                std::make_pair("over", AlphaBlending::Over),
            };

            static const std::unordered_map<std::string, DepthMode> NAME_TO_DEPTH_MODE_MAP = {
                std::make_pair("none",       DepthMode::None),
                std::make_pair("read",       DepthMode::Read),
                std::make_pair("read_write", DepthMode::ReadWrite),
            };
            
            static const std::unordered_map<std::string, DepthTest> NAME_TO_DEPTH_TEST_MAP = {
                std::make_pair("greater_or_equal",  DepthTest::GreaterOrEqual),
                std::make_pair("equal",             DepthTest::Equal),
            };

            static const std::unordered_map<std::string, FaceCullMode> NAME_TO_CULL_MODE_MAP = {
                std::make_pair("none",  FaceCullMode::None),
                std::make_pair("front", FaceCullMode::Front),
                std::make_pair("back",  FaceCullMode::Back),
            };

            static const std::unordered_map<std::string, PrimitiveKind> NAME_TO_PRIMITIVE_MAP = {
                std::make_pair("triangle",  PrimitiveKind::Triangle),
                std::make_pair("point",     PrimitiveKind::Point),
            };

            auto& rasterization = json["rasterization"];
            
            if (rasterization.contains("alpha_blending"))
                alphaBlending = NAME_TO_BLENDING_MAP.at(rasterization["alpha_blending"]);
            if (rasterization.contains("depth_mode"))
                depthMode = NAME_TO_DEPTH_MODE_MAP.at(rasterization["depth_mode"]);
            if (rasterization.contains("depth_test"))
                depthTest = NAME_TO_DEPTH_TEST_MAP.at(rasterization["depth_test"]);
            if (rasterization.contains("cull_mode"))
                cullMode = NAME_TO_CULL_MODE_MAP.at(rasterization["cull_mode"]);
            if (rasterization.contains("primitive_kind"))
                primitiveKind = NAME_TO_PRIMITIVE_MAP.at(rasterization["primitive_kind"]);
            if (rasterization.contains("depth_clamp"))
                clampDepth = rasterization["depth_clamp"];

            colorFormats.reserve(rasterization["colors"].size());
            for (auto& color : rasterization["colors"])
                colorFormats.push_back(FormatUtils::formatFromString(color));
            if (rasterization.contains("depth"))
                depthFormat = FormatUtils::formatFromString(rasterization["depth"]);
        }

        const Pipeline pipeline = Device::CreatePipeline({
            .PipelineLayout = shaderTemplate->GetPipelineLayout(),
            .Shaders = shaderTemplate->GetReflection().Shaders(),
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
            .ClampDepth = overrides.PipelineOverrides.ClampDepth.value_or(clampDepth)},
            Device::DummyDeletionQueue());
        Device::NamePipeline(pipeline, name.Name.AsStringView());

        return pipeline;
    };

    PipelineInfo shader = {};
    shader.PipelineTemplate = shaderTemplate;
    shader.Layout = shaderTemplate->GetPipelineLayout();
    shader.BindlessName = referencedBindlessSet;
    shader.BindlessCount =
        shaderTemplate->GetReflection().DescriptorSetsInfo()[BINDLESS_DESCRIPTORS_INDEX].HasBindless ?
        json.value("bindless_count", 0u) : 0u;

    if (enumHasAny(allocationHint, ShaderCacheAllocationType::Pipeline))
        shader.Pipeline = createPipeline();

    return shader;
}

namespace 
{
constexpr Format formatFromAssetImageFormat(assetlib::ShaderImageFormat format)
{
    static_assert((u8)Format::MaxVal == (u8)assetlib::ShaderImageFormat::MaxVal);
    static_assert((u8)Format::Undefined == (u8)assetlib::ShaderImageFormat::Undefined);
    static_assert((u8)Format::R8_UNORM == (u8)assetlib::ShaderImageFormat::R8_UNORM);
    static_assert((u8)Format::R8_SNORM == (u8)assetlib::ShaderImageFormat::R8_SNORM);
    static_assert((u8)Format::R8_UINT == (u8)assetlib::ShaderImageFormat::R8_UINT);
    static_assert((u8)Format::R8_SINT == (u8)assetlib::ShaderImageFormat::R8_SINT);
    static_assert((u8)Format::R8_SRGB == (u8)assetlib::ShaderImageFormat::R8_SRGB);
    static_assert((u8)Format::RG8_UNORM == (u8)assetlib::ShaderImageFormat::RG8_UNORM);
    static_assert((u8)Format::RG8_SNORM == (u8)assetlib::ShaderImageFormat::RG8_SNORM);
    static_assert((u8)Format::RG8_UINT == (u8)assetlib::ShaderImageFormat::RG8_UINT);
    static_assert((u8)Format::RG8_SINT == (u8)assetlib::ShaderImageFormat::RG8_SINT);
    static_assert((u8)Format::RG8_SRGB == (u8)assetlib::ShaderImageFormat::RG8_SRGB);
    static_assert((u8)Format::RGBA8_UNORM == (u8)assetlib::ShaderImageFormat::RGBA8_UNORM);
    static_assert((u8)Format::RGBA8_SNORM == (u8)assetlib::ShaderImageFormat::RGBA8_SNORM);
    static_assert((u8)Format::RGBA8_UINT == (u8)assetlib::ShaderImageFormat::RGBA8_UINT);
    static_assert((u8)Format::RGBA8_SINT == (u8)assetlib::ShaderImageFormat::RGBA8_SINT);
    static_assert((u8)Format::RGBA8_SRGB == (u8)assetlib::ShaderImageFormat::RGBA8_SRGB);
    static_assert((u8)Format::R16_UNORM == (u8)assetlib::ShaderImageFormat::R16_UNORM);
    static_assert((u8)Format::R16_SNORM == (u8)assetlib::ShaderImageFormat::R16_SNORM);
    static_assert((u8)Format::R16_UINT == (u8)assetlib::ShaderImageFormat::R16_UINT);
    static_assert((u8)Format::R16_SINT == (u8)assetlib::ShaderImageFormat::R16_SINT);
    static_assert((u8)Format::R16_FLOAT == (u8)assetlib::ShaderImageFormat::R16_FLOAT);
    static_assert((u8)Format::RG16_UNORM == (u8)assetlib::ShaderImageFormat::RG16_UNORM);
    static_assert((u8)Format::RG16_SNORM == (u8)assetlib::ShaderImageFormat::RG16_SNORM);
    static_assert((u8)Format::RG16_UINT == (u8)assetlib::ShaderImageFormat::RG16_UINT);
    static_assert((u8)Format::RG16_SINT == (u8)assetlib::ShaderImageFormat::RG16_SINT);
    static_assert((u8)Format::RG16_FLOAT == (u8)assetlib::ShaderImageFormat::RG16_FLOAT);
    static_assert((u8)Format::RGBA16_UNORM == (u8)assetlib::ShaderImageFormat::RGBA16_UNORM);
    static_assert((u8)Format::RGBA16_SNORM == (u8)assetlib::ShaderImageFormat::RGBA16_SNORM);
    static_assert((u8)Format::RGBA16_UINT == (u8)assetlib::ShaderImageFormat::RGBA16_UINT);
    static_assert((u8)Format::RGBA16_SINT == (u8)assetlib::ShaderImageFormat::RGBA16_SINT);
    static_assert((u8)Format::RGBA16_FLOAT == (u8)assetlib::ShaderImageFormat::RGBA16_FLOAT);
    static_assert((u8)Format::R32_UINT == (u8)assetlib::ShaderImageFormat::R32_UINT);
    static_assert((u8)Format::R32_SINT == (u8)assetlib::ShaderImageFormat::R32_SINT);
    static_assert((u8)Format::R32_FLOAT == (u8)assetlib::ShaderImageFormat::R32_FLOAT);
    static_assert((u8)Format::RG32_UINT == (u8)assetlib::ShaderImageFormat::RG32_UINT);
    static_assert((u8)Format::RG32_SINT == (u8)assetlib::ShaderImageFormat::RG32_SINT);
    static_assert((u8)Format::RG32_FLOAT == (u8)assetlib::ShaderImageFormat::RG32_FLOAT);
    static_assert((u8)Format::RGB32_UINT == (u8)assetlib::ShaderImageFormat::RGB32_UINT);
    static_assert((u8)Format::RGB32_SINT == (u8)assetlib::ShaderImageFormat::RGB32_SINT);
    static_assert((u8)Format::RGB32_FLOAT == (u8)assetlib::ShaderImageFormat::RGB32_FLOAT);
    static_assert((u8)Format::RGBA32_UINT == (u8)assetlib::ShaderImageFormat::RGBA32_UINT);
    static_assert((u8)Format::RGBA32_SINT == (u8)assetlib::ShaderImageFormat::RGBA32_SINT);
    static_assert((u8)Format::RGBA32_FLOAT == (u8)assetlib::ShaderImageFormat::RGBA32_FLOAT);
    static_assert((u8)Format::RGB10A2 == (u8)assetlib::ShaderImageFormat::RGB10A2);
    static_assert((u8)Format::R11G11B10 == (u8)assetlib::ShaderImageFormat::R11G11B10);
    static_assert((u8)Format::D32_FLOAT == (u8)assetlib::ShaderImageFormat::D32_FLOAT);
    static_assert((u8)Format::D24_UNORM_S8_UINT == (u8)assetlib::ShaderImageFormat::D24_UNORM_S8_UINT);
    static_assert((u8)Format::D32_FLOAT_S8_UINT == (u8)assetlib::ShaderImageFormat::D32_FLOAT_S8_UINT);
    
    return (Format)format;
}
constexpr DynamicStates dynamicStatesFromAssetDynamicStates(const assetlib::ShaderRasterizationDynamicStates& states)
{
    static_assert((u8)DynamicStates::None == (u8)assetlib::ShaderRasterizationDynamicState::None);
    static_assert((u8)DynamicStates::Viewport == BIT((u8)assetlib::ShaderRasterizationDynamicState::Viewport));
    static_assert((u8)DynamicStates::Scissor ==  BIT((u8)assetlib::ShaderRasterizationDynamicState::Scissor));
    static_assert((u8)DynamicStates::DepthBias ==  BIT((u8)assetlib::ShaderRasterizationDynamicState::DepthBias));

    DynamicStates dynamicStates = DynamicStates::None;
    for (auto state : states)
        dynamicStates |= DynamicStates(BIT((u8)state));

    return dynamicStates;
}
constexpr AlphaBlending alphaBlendingFromAssetAlphaBlending(assetlib::ShaderRasterizationAlphaBlending blending)
{
    static_assert((u8)AlphaBlending::None ==  (u8)assetlib::ShaderRasterizationAlphaBlending::None);
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
constexpr PrimitiveKind primitiveKindModeFromAssetPrimitiveKind(assetlib::ShaderRasterizationPrimitiveKind primitive)
{
    static_assert((u8)PrimitiveKind::Triangle == (u8)assetlib::ShaderRasterizationPrimitiveKind::Triangle);
    static_assert((u8)PrimitiveKind::Point == (u8)assetlib::ShaderRasterizationPrimitiveKind::Point);

    return (PrimitiveKind)primitive;
}
}

// todo: rename once ready
std::optional<ShaderCache::PipelineInfo> ShaderCache::TryCreateSlangPipeline(const ShaderNameWithOverrides& name,
    ShaderOverridesView& overrides, ShaderCacheAllocationType allocationHint)
{
    const std::string& path = m_ShaderNameToPath.at(name.Name);

    auto shaderLoadInfo = assetlib::shader::readLoadInfo(path);
    if (!shaderLoadInfo.has_value())
        return std::nullopt;
    
    StringId referencedBindlessSet = {};
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> descriptorsLayoutsOverrides{};
    if (shaderLoadInfo->BindlessSetReference.has_value())
    {
        referencedBindlessSet = StringId::FromString(*shaderLoadInfo->BindlessSetReference);
        if (!m_PersistentDescriptors.contains(referencedBindlessSet))
            return std::nullopt;
        
        descriptorsLayoutsOverrides[BINDLESS_DESCRIPTORS_INDEX] =
            m_PersistentDescriptors.at(referencedBindlessSet).Layout;
    }

    const ShaderPipelineTemplate* shaderTemplate =
        GetShaderPipelineTemplate(name, overrides, path, descriptorsLayoutsOverrides);
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

            colorFormats.reserve(rasterization.ColorFormats.size());
            for (auto format : rasterization.ColorFormats)
                colorFormats.push_back(formatFromAssetImageFormat(format));
            if (rasterization.DepthFormat.has_value())
                depthFormat = formatFromAssetImageFormat(*rasterization.DepthFormat);
        }
        
        const Pipeline pipeline = Device::CreatePipeline({
            .PipelineLayout = shaderTemplate->GetPipelineLayout(),
            .Shaders = shaderTemplate->GetReflection().Shaders(),
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
            .ClampDepth = overrides.PipelineOverrides.ClampDepth.value_or(clampDepth)},
            Device::DummyDeletionQueue());
        Device::NamePipeline(pipeline, name.Name.AsStringView());

        return pipeline;
    };

    PipelineInfo shader = {};
    shader.PipelineTemplate = shaderTemplate;
    shader.Layout = shaderTemplate->GetPipelineLayout();
    shader.BindlessName = referencedBindlessSet;
    shader.BindlessCount =
        shaderTemplate->GetReflection().DescriptorSetsInfo()[BINDLESS_DESCRIPTORS_INDEX].HasBindless ?
        shaderLoadInfo->BindlessCount.value_or(0u) : 0u;

    if (enumHasAny(allocationHint, ShaderCacheAllocationType::Pipeline))
        shader.Pipeline = createPipeline();

    return shader;
}

const ShaderPipelineTemplate* ShaderCache::GetShaderPipelineTemplate(const ShaderNameWithOverrides& name,
    const ShaderOverridesView& overrides, std::vector<std::string>& stages,
    const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides)
{
    ShaderStageConverter::Options options = {};
    if (overrides.Defines.Hash != 0)
    {
        options.DefinesHash = overrides.Defines.Hash;
        options.Defines.reserve(overrides.Defines.Defines.size());
        for (auto& define : overrides.Defines.Defines)
            options.Defines.emplace_back(define.Name.AsStringView(), define.Value);
    }        

    std::array<ShaderStage, MAX_PIPELINE_SHADER_COUNT> shaderStages{};
    std::array<std::string, MAX_PIPELINE_SHADER_COUNT> shaderEntryPoints{};
    for (auto&& [i, stage]: std::views::enumerate(stages))
    {
        assetLib::File shaderFile;
        assetLib::loadAssetFile(stage, shaderFile);
        assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);
        shaderStages[i] = ShaderReflection::GetShaderStage(shaderInfo.ShaderStages);
        shaderEntryPoints[i] = "main";
        
        std::filesystem::path stagePath = stage;
        stagePath.replace_filename(ShaderStageConverter::GetBakedFileName(shaderInfo.OriginalFile, options));
        stagePath.replace_extension(ShaderStageConverter::POST_CONVERT_EXTENSION);
        stage = stagePath.string();
        if (ShaderStageConverter::NeedsConversion(
            *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), shaderInfo.OriginalFile, options))
        {
            const auto baked = ShaderStageConverter::Bake(
                *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), shaderInfo.OriginalFile, options);
            if (!baked && !std::filesystem::exists(stage))
                return nullptr;
        }
    }

    const std::string shaderReflectionKey = AssetManager::GetShaderKey(stages);
    ShaderReflection* reflection = AssetManager::AddShader(shaderReflectionKey, ShaderReflection::ReflectFrom(stages));

    auto& pipelineTemplate = m_ShaderPipelineTemplates[name] = ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo{
        .ShaderReflection = reflection,
        .ShaderStages = Span(shaderStages.data(), stages.size()),
        .ShaderEntryPoints = Span(shaderEntryPoints.data(), stages.size()),
        .DescriptorLayoutOverrides = descriptorLayoutOverrides
    });
    
    return &pipelineTemplate;
}

const ShaderPipelineTemplate* ShaderCache::GetShaderPipelineTemplate(const ShaderNameWithOverrides& name,
    const ShaderOverridesView& overrides, const std::filesystem::path& path,
    const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides)
{
    std::vector<std::pair<std::string, std::string>> defines;
    defines.reserve(overrides.Defines.Defines.size());
    for (auto& define : overrides.Defines.Defines)
        defines.emplace_back(define.Name.AsStringView(), define.Value);
    
    bakers::Slang baker;
    bakers::SlangBakeSettings settings = {
        .Defines = defines,
        .DefinesHash = overrides.Defines.Hash,
        .IncludePaths = m_BakeSettings->IncludePaths
    };
    auto baked = baker.BakeToFile(path, settings, *m_BakersCtx);
    if (!baked)
        return nullptr;

    auto shaderReflectionResult = ShaderReflection::ReflectFromSlang(baker.GetBakedPath(path, settings, *m_BakersCtx));
    if (!shaderReflectionResult.has_value())
        return nullptr;

    ShaderReflectionEntryPointsInfo entryPointsInfo = ShaderReflection::GetEntryPointsInfo(baked->Header);

    ShaderReflection* reflection =
        AssetManager::AddShader(
            std::format("{}.{}", name.Name, name.OverridesHash), std::move(*shaderReflectionResult));

    return &(m_ShaderPipelineTemplates[name] = ShaderPipelineTemplate({
        .ShaderReflection = reflection,
        .ShaderStages = Span(entryPointsInfo.Stages.data(), entryPointsInfo.Count),
        .ShaderEntryPoints = Span(entryPointsInfo.Names.data(), entryPointsInfo.Count),
        .DescriptorLayoutOverrides = descriptorLayoutOverrides
    }));
}
