#include "ShaderCache.h"

#include "AssetManager.h"
#include "core.h"
#include "Converters.h"
#include "cvars/CVarSystem.h"
#include "Vulkan/Device.h"

#include <fstream>
#include <nlohmann/json.hpp>

void ShaderCache::Init()
{
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
    ShaderCacheAllocationHint allocationHint)
{
    return Allocate(name, {}, allocators, allocationHint);
}

ShaderCacheAllocateResult ShaderCache::Allocate(StringId name, ShaderOverridesView&& overrides,
    DescriptorArenaAllocators& allocators, ShaderCacheAllocationHint allocationHint)
{
    const ShaderNameWithOverrides nameWithOverrides{.Name = name, .OverridesHash = overrides.Hash};

    Shader shader = {};

    const bool hasPipeline = m_Pipelines.contains(nameWithOverrides);
    if (!hasPipeline || m_Pipelines.at(nameWithOverrides).ShouldReload)
    {
        const std::optional<PipelineInfo> pipelineInfo = TryCreatePipeline(
            nameWithOverrides, overrides, allocationHint);
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
            allocators.Get((DescriptorsKind)i),
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
    namespace fs = std::filesystem;
    for (auto& file : fs::recursive_directory_iterator(
        *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv)))
    {
        if (file.is_directory())
            continue;

        auto& path = file.path();

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
    ShaderOverridesView& overrides, ShaderCacheAllocationHint allocationHint)
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

    if (enumHasAny(allocationHint, ShaderCacheAllocationHint::Pipeline))
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

    for (auto& stage : stages)
    {
        assetLib::File shaderFile;
        assetLib::loadAssetFile(stage, shaderFile);
        assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);

        std::filesystem::path stagePath = stage;
        stagePath.replace_filename(ShaderStageConverter::GetBakedFileName(shaderInfo.OriginalFile, options));
        stagePath.replace_extension(ShaderStageConverter::POST_CONVERT_EXTENSION);
        stage = stagePath.string();
        if (ShaderStageConverter::NeedsConversion(
            *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), shaderInfo.OriginalFile, options))
        {
            const auto backed = ShaderStageConverter::Bake(
                *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), shaderInfo.OriginalFile, options);
            if (!backed && !std::filesystem::exists(stage))
                return nullptr;
        }
    }

    const std::string shaderReflectionKey = AssetManager::GetShaderKey(stages);
    ShaderReflection* reflection = AssetManager::AddShader(shaderReflectionKey, ShaderReflection::ReflectFrom(stages));

    auto& pipelineTemplate = m_ShaderPipelineTemplates[name] = ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo{
        .ShaderReflection = reflection,
        .DescriptorLayoutOverrides = descriptorLayoutOverrides
    });
    
    return &pipelineTemplate;
}