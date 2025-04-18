﻿#include "ShaderCache.h"

#include "AssetManager.h"
#include "core.h"
#include "Converters.h"
#include "cvars/CVarSystem.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <queue>
#include <memory>
#include <unordered_set>
#include <efsw/efsw.hpp>

DescriptorArenaAllocators* ShaderCache::s_Allocators = {nullptr};
StringUnorderedMap<ShaderCache::FileNode> ShaderCache::s_FileGraph = {};
StringUnorderedMap<ShaderCache::Record> ShaderCache::s_Records = {};    
std::unordered_map<StringId, Shader*> ShaderCache::s_ShadersMap = {};    
std::vector<std::unique_ptr<Shader>> ShaderCache::s_Shaders = {};
std::vector<ShaderCache::PipelineData> ShaderCache::s_Pipelines = {};
std::unordered_map<StringId, Descriptors> ShaderCache::s_BindlessDescriptors = {};
DeletionQueue* ShaderCache::s_FrameDeletionQueue = {};

namespace
{
    std::mutex g_FileUpdateMutex;
}
std::vector<std::pair<std::string, std::string>> ShaderCache::s_ToRename = {};
std::vector<std::filesystem::path> ShaderCache::s_ToReload = {};

struct ShaderCache::FileWatcher
{
    efsw::FileWatcher Watcher;
    std::shared_ptr<efsw::FileWatchListener> Listener;

    FileWatcher() = default;
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = default;
    FileWatcher& operator=(FileWatcher&&) = default;
    ~FileWatcher()
    {
        Watcher.removeWatch(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv));
    }
};
std::unique_ptr<ShaderCache::FileWatcher> ShaderCache::s_FileWatcher = {};

Shader::Shader(u32 pipelineIndex,
    const std::array<::Descriptors, MAX_DESCRIPTOR_SETS>& descriptors)
        : m_Pipeline(pipelineIndex), m_Descriptors(descriptors)
{
}

Pipeline Shader::Pipeline() const
{
    return ShaderCache::s_Pipelines[m_Pipeline].Pipeline;
}

PipelineLayout Shader::GetLayout() const
{
    return ShaderCache::s_Pipelines[m_Pipeline].PipelineLayout;
}

ShaderOverridesView Shader::CopyOverrides() const
{
    ShaderOverridesView view = {};
    view.Hash = ShaderCache::s_Pipelines[m_Pipeline].OverridesHash;
    
    return view;
}

void ShaderCache::Init()
{
    InitFileWatcher();
    CreateFileGraph();
}

void ShaderCache::Shutdown()
{
    std::vector deleted(s_Shaders.size(), false);
    for (auto& s : s_Shaders)
    {
        if (deleted[s->m_Pipeline])
            continue;

        deleted[s->m_Pipeline] = true;
        Device::Destroy(s->Pipeline());
    }
    s_FileWatcher.reset();
}

void ShaderCache::OnFrameBegin(FrameContext& ctx)
{
    s_FrameDeletionQueue = &ctx.DeletionQueue;

    std::lock_guard lock(g_FileUpdateMutex);

    for (auto&& [newName, oldName] : s_ToRename)
        HandleRename(newName, oldName);

    for (auto& path : s_ToReload)
    {
        std::string extension = path.extension().string();
        if (extension == SHADER_EXTENSION)
            HandleShaderModification(path.string());
        else if (ShaderStageConverter::WatchesExtension(extension))
            HandleStageModification(path.string());
        else if (extension == SHADER_HEADER_EXTENSION)
            HandleHeaderModification(path.string());
    }

    s_ToRename.clear();
    s_ToReload.clear();
}

void ShaderCache::AddBindlessDescriptors(StringId name, const Descriptors& descriptors)
{
    s_BindlessDescriptors[name] = descriptors;
}

const Shader& ShaderCache::Get(StringId name)
{
    return *s_ShadersMap.find(name)->second;
}

const Shader& ShaderCache::Register(StringId name, std::string_view path, ShaderOverridesView&& overrides)
{
    std::string fullPath = *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv) + std::string{path};
    
    ShaderProxy shaderProxy = {};
    u32 pipeline = {};
    
    if (!s_Records.contains(fullPath))
    {
        /* if this is completely new shader */
        pipeline = (u32)s_Pipelines.size();
        const u64 overridesHash = overrides.Hash;
        shaderProxy = ReloadShader(fullPath, ReloadType::PipelineDescriptors, std::move(overrides));
        s_Pipelines.push_back({
            .Pipeline = shaderProxy.Pipeline,
            .PipelineLayout = shaderProxy.PipelineLayout,
            .OverridesHash = overridesHash});
    }
    else
    {
        auto& shaders = s_Records.find(fullPath)->second.Shaders;
        
        return Register(name, shaders.front(), std::move(overrides));
    }   

    return AddShader(name, pipeline, shaderProxy, fullPath);
}

const Shader& ShaderCache::Register(StringId name, const Shader* shader, ShaderOverridesView&& overrides)
{
    /* if shader already exists in some form, then two cases are possible:
     * 1) to-be-registered shader is identical to previously loaded one, but has different `name`
     *   in this case we can copy pipeline of the existing shader, and just allocate descriptors
     * 2) to-be-registered shader is different (has overrides), in this case we have two more cases:
     *   2a) to-be-registered was already loaded for `name`, in this case we do not need to allocate descriptors
     *   2b) to-be-registered was not loaded for `name`, in this case we have to load pipeline and descriptors
     *
     *   case 2a) is an early exit, since it does not produce any entries in `s_Shaders` and other arrays
     */

    const u64 overridesHash = overrides.Hash;
    ShaderProxy shaderProxy = {};
    /* 2a) */
    if (s_ShadersMap.contains(name))
    {
        if (s_Pipelines[s_ShadersMap.find(name)->second->m_Pipeline].OverridesHash == overridesHash)
            return *s_ShadersMap.find(name)->second;

        shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Pipeline, std::move(overrides));
        
        s_Pipelines[s_ShadersMap.find(name)->second->m_Pipeline] = {
            .Pipeline = shaderProxy.Pipeline,
            .PipelineLayout = shaderProxy.PipelineLayout,
            .OverridesHash = overridesHash};
            
        return *s_ShadersMap.find(name)->second;
    }

    u32 pipeline = {};

    /* 1) */
    if (s_Pipelines[shader->m_Pipeline].OverridesHash == overridesHash)
    {
        pipeline = shader->m_Pipeline;
        shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Descriptors, std::move(overrides));
    }
    /* 2b) */
    else
    {
        pipeline = (u32)s_Pipelines.size();
        shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::PipelineDescriptors, std::move(overrides));
        s_Pipelines.push_back({
            .Pipeline = shaderProxy.Pipeline,
            .PipelineLayout = shaderProxy.PipelineLayout,
            .OverridesHash = overridesHash});
    }

    return AddShader(name, pipeline, shaderProxy, shader->m_FilePath);
}

const Shader& ShaderCache::AddShader(StringId name, u32 pipeline, const ShaderProxy& proxy,
    std::string_view path)
{
    s_Shaders.push_back(std::make_unique<Shader>(pipeline, proxy.Descriptors));
    s_Shaders.back()->m_FilePath = path;

    s_ShadersMap.emplace(name, s_Shaders.back().get());
    for (auto& dependency : proxy.Dependencies)
        s_Records[dependency].Shaders.push_back(s_Shaders.back().get());

    return *s_Shaders.back();
}

void ShaderCache::HandleRename(std::string_view newPath, std::string_view oldPath)
{
    if (s_Records.find(oldPath)->second.Shaders.front()->m_FilePath == oldPath)
        s_Records.find(oldPath)->second.Shaders.front()->m_FilePath = newPath;
    auto records = s_Records.extract(oldPath);
    records.key() = newPath;
    s_Records.insert(std::move(records));
}

void ShaderCache::HandleShaderModification(std::string_view path)
{
    auto it = s_Records.find(path);
    if (it == s_Records.end())
        return;
    
    const Record& record = it->second;
    std::unordered_set<Shader*> handled = {};
    std::vector deletedPipelines(s_Pipelines.size(), false);
    for (auto* shader : record.Shaders)
    {
        if (handled.contains(shader))
            continue;
        handled.emplace(shader);

        u32 pipelineIndex = shader->m_Pipeline;
        if (deletedPipelines[pipelineIndex])
            continue;
        deletedPipelines[pipelineIndex] = true;
        s_FrameDeletionQueue->Enqueue(s_Pipelines[pipelineIndex].Pipeline);
        
        /* when the `ShaderOverrides` has non-zero hash, it means that there are some overloads,
         * so this Reload operation will be useless, as we will have to reload it once more
         * with correct overloads; so instead we simply set pipeline overload hash to zero, thus
         * triggering reload on the next access operation
         */
        if (s_Pipelines[pipelineIndex].OverridesHash != 0)
        {
            s_Pipelines[pipelineIndex].OverridesHash = 0;
            continue;
        }
        
        ShaderProxy shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Pipeline, {});
        s_Pipelines[pipelineIndex].Pipeline = shaderProxy.Pipeline;
        s_Pipelines[pipelineIndex].PipelineLayout = shaderProxy.PipelineLayout;
    }
}

void ShaderCache::HandleStageModification(std::string_view path)
{
    auto& stages = s_FileGraph.find(path)->second.Files;
    
    auto baked = ShaderStageConverter::Bake(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), path);
    if (baked.has_value())
        HandleShaderModification(stages.front().Processed);
}

void ShaderCache::HandleHeaderModification(std::string_view path)
{
    auto& stages = s_FileGraph.find(path)->second.Files;
    for (auto& stage : stages)
    {
        auto baked = ShaderStageConverter::Bake(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), stage.Raw);
        if (baked.has_value())
            HandleShaderModification(stage.Processed);
    }
}

void ShaderCache::CreateFileGraph()
{
    for (auto& file : std::filesystem::recursive_directory_iterator(
        *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv)))
    {
        if (file.is_directory())
            continue;

        auto& path = file.path();
        if (path.extension().string() != ShaderStageConverter::POST_CONVERT_EXTENSION)
            continue;

        assetLib::File shaderFile;
        assetLib::loadAssetFile(path.string(), shaderFile);
        assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);

        for (auto& include : shaderInfo.IncludedFiles)
            s_FileGraph[include].Files.push_back({
                .Raw = shaderInfo.OriginalFile,
                .Processed = path.generic_string()});
        s_FileGraph[shaderInfo.OriginalFile].Files.push_back({
            .Raw = shaderInfo.OriginalFile,
            .Processed = path.generic_string()});
    }
}

ShaderCache::ShaderProxy ShaderCache::ReloadShader(std::string_view path, ReloadType reloadType,
    ShaderOverridesView&& overrides)
{
    std::ifstream in(path.data());
    nlohmann::json json = nlohmann::json::parse(in);

    StringId name = StringId::FromString(json["name"]);

    std::vector<std::string> stages;
    stages.reserve(json["shader_stages"].size());
    for (auto& stage : json["shader_stages"])
        stages.push_back(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv) + std::string{stage});

    if (overrides.Defines.Hash != 0)
    {
        ShaderStageConverter::Options options;
        options.DefinesHash = overrides.Defines.Hash;
        options.Defines.reserve(overrides.Defines.Defines.size());
        for (auto& define : overrides.Defines.Defines)
            options.Defines.emplace_back(define.Name.AsStringView(), define.Value);
        
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
                ShaderStageConverter::Bake(
                    *CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv), shaderInfo.OriginalFile, options);
        }
    }

    AssetManager::RemoveShader(AssetManager::GetShaderKey(stages));
    ShaderPipelineTemplate* shaderTemplate =
        ShaderTemplateLibrary::ReloadShaderPipelineTemplate(stages, name, *s_Allocators);

    ShaderProxy shader = {};
    shader.Dependencies.reserve(stages.size() + 1);
    for (auto& stage : stages)
        shader.Dependencies.emplace_back(stage);
    shader.Dependencies.emplace_back(path);
    
    if (reloadType == ReloadType::PipelineDescriptors || reloadType == ReloadType::Pipeline)
    {
        std::vector<Format> colorFormats;
        std::optional<Format> depthFormat;
        DynamicStates dynamicStates = DynamicStates::Default;
        AlphaBlending alphaBlending = AlphaBlending::Over;
        DepthMode depthMode = DepthMode::ReadWrite;
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

        shader.PipelineLayout = shaderTemplate->GetPipelineLayout();
        shader.Pipeline = Device::CreatePipeline({
            .PipelineLayout = shader.PipelineLayout,
            .Shaders = shaderTemplate->GetReflection().Shaders(),
            .ColorFormats = colorFormats,
            .DepthFormat = depthFormat ? *depthFormat : Format::Undefined,
            .DynamicStates = overrides.PipelineOverrides.DynamicStates.value_or(dynamicStates),
            .DepthMode = overrides.PipelineOverrides.DepthMode.value_or(depthMode),
            .CullMode = overrides.PipelineOverrides.CullMode.value_or(cullMode),
            .AlphaBlending = overrides.PipelineOverrides.AlphaBlending.value_or(alphaBlending),
            .PrimitiveKind = overrides.PipelineOverrides.PrimitiveKind.value_or(primitiveKind),
            .Specialization = overrides.Specializations.ToPipelineSpecializationsView(*shaderTemplate),
            .IsComputePipeline = shaderTemplate->IsComputeTemplate(),
            .UseDescriptorBuffer = true,
            .ClampDepth = overrides.PipelineOverrides.ClampDepth.value_or(clampDepth)},
            Device::DummyDeletionQueue());
    }

    if (reloadType == ReloadType::Pipeline)
        return shader;
    /* basically there is no way to actually reload descriptors info
    * the descriptors filled by c++ code, and it is not really possible to make it work
    * w/o rewriting that code, so the descriptors are loaded once with the shader (no hot-reload for them)
    * changing the descriptors while application is running will lead to undefined behaviour
    */
    auto setPresence = shaderTemplate->GetSetPresence();
    const bool referencesOtherBindlessSet = json.contains("bindless");
    for (u32 i = 0; i <= BINDLESS_DESCRIPTORS_INDEX; i++)
    {
        if (!setPresence[i])
            continue;

        if (i == BINDLESS_DESCRIPTORS_INDEX && referencesOtherBindlessSet)
            continue;
        
        std::optional<Descriptors> descriptors = Device::AllocateDescriptors(
            shaderTemplate->GetAllocator(i == (u32)DescriptorsKind::Sampler ?
                DescriptorsKind::Sampler : DescriptorsKind::Resource),
            shaderTemplate->GetDescriptorsLayout(i), {
                .Bindings = shaderTemplate->GetReflection().DescriptorSetsInfo()[i].Descriptors,
                .BindlessCount = shaderTemplate->GetReflection().DescriptorSetsInfo()[i].HasBindless ?
                    (u32)json["bindless_count"] : 0});
        ASSERT(descriptors.has_value(), "Increase allocator size")

        shader.Descriptors[i] = *descriptors;
    }

    if (referencesOtherBindlessSet)
        shader.Descriptors[BINDLESS_DESCRIPTORS_INDEX] =
            {s_BindlessDescriptors.at(StringId::FromString(json["bindless"]))};
    
    return shader;
}

void ShaderCache::InitFileWatcher()
{
    class Listener : public efsw::FileWatchListener
    {
    private:
        struct FileUpdateData
        {
            std::string FileName;
            std::string OldFileName;
            efsw::Action Action;
            std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
        };
    public:
        void handleFileAction(efsw::WatchID watchId, const std::string& directory, const std::string& filename,
            efsw::Action action, std::string oldFilename) override
        {
            std::string filePath = std::filesystem::weakly_canonical(directory + filename).generic_string();
            switch (action)
            {
            /* `moved` is a rename (the efsw comment seems to be wrong) */
            case efsw::Actions::Moved:
            case efsw::Actions::Modified:
                {
                    std::scoped_lock lock(m_Mutex);
                    if (m_PendingFileNames.contains(filePath) && action == efsw::Action::Modified)
                        break;
                    m_PendingFileNames.emplace(filePath);
                    m_FilesToProcess.push({
                        .FileName = filePath,
                        .OldFileName = std::filesystem::weakly_canonical(directory + oldFilename).generic_string(),
                        .Action = action,
                        .TimePoint = std::chrono::high_resolution_clock::now()});
                }
                m_ConditionVariable.notify_one();
                break;
            /* at the moment we just ignore `delete` and `add` actions */
            case efsw::Actions::Add:
            case efsw::Actions::Delete:
            default:
                break;
            }
        }
        static void StartDebounceThread(std::shared_ptr<Listener> listener)
        {
            using namespace std::chrono;
            using namespace std::chrono_literals;

            static constexpr milliseconds DEBOUNCE_TIME = 85ms;

            std::thread debounce([listener]()
            {
                std::weak_ptr listenerWeak = listener;

                for (;;)
                {
                    std::shared_ptr<Listener> lockedListener = listenerWeak.lock();
                    if (!lockedListener)
                        break;

                    std::unique_lock lock(lockedListener->m_Mutex);
                    lockedListener->m_ConditionVariable.wait(lock, [&]()
                    {
                        return !lockedListener->m_FilesToProcess.empty();
                    });

                    milliseconds delta = duration_cast<milliseconds>(
                        high_resolution_clock::now() -
                        lockedListener->m_FilesToProcess.front().TimePoint);
                    if (delta < DEBOUNCE_TIME)
                    {
                        lock.unlock();
                        /* we can just sleep, because all other elements in the queue are newer */
                        std::this_thread::sleep_for(DEBOUNCE_TIME - delta);
                    }

                    FileUpdateData fileUpdateData = lockedListener->m_FilesToProcess.front();
                    std::string fileName = fileUpdateData.FileName;
                    lockedListener->m_FilesToProcess.pop();
                    lockedListener->m_PendingFileNames.erase(fileName);

                    lockedListener->ProcessResource(fileUpdateData);
                }
            });

            debounce.detach();
        }
    private:
        void ProcessResource(const FileUpdateData& fileUpdateData)
        {
            std::filesystem::path filePath = fileUpdateData.FileName;

            /* is it possible that file is deleted or renamed before we begin to process it */
            if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath))
                return;

            std::lock_guard lock(g_FileUpdateMutex);

            if (fileUpdateData.Action == efsw::Action::Moved)
            {
                s_ToRename.emplace_back(fileUpdateData.FileName, fileUpdateData.OldFileName);
                return;
            }

            ASSERT(fileUpdateData.Action == efsw::Action::Modified, "Unexpected file update action")
            s_ToReload.emplace_back(filePath);
        }
    private:
        /* some editors (like vs code) do something strange when file is updated
         * e.g. it is updated twice on save, in order for us not to bake resource more times
         * than necessary we order baking only after some delay
         */
        std::queue<FileUpdateData> m_FilesToProcess;
        std::unordered_set<std::string> m_PendingFileNames;

        std::mutex m_Mutex;
        std::condition_variable m_ConditionVariable;
    };

    static constexpr bool IS_RECURSIVE = true;
    s_FileWatcher = std::make_unique<FileWatcher>();
    std::shared_ptr<Listener> listener = std::make_shared<Listener>();
    s_FileWatcher->Listener = listener;
    Listener::StartDebounceThread(listener);
    s_FileWatcher->Watcher.addWatch(*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv),
        s_FileWatcher->Listener.get(), IS_RECURSIVE);
    s_FileWatcher->Watcher.watch();
}
