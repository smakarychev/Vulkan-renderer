#include "ShaderCache.h"

#include <fstream>
#include <nlm_json.hpp>
#include <queue>
#include <memory>
#include <unordered_set>
#include <efsw/efsw.hpp>

#include "AssetManager.h"
#include "Core/core.h"
#include "Converters.h"
#include "cvars/CVarSystem.h"

DescriptorArenaAllocators* ShaderCache::s_Allocators = {nullptr};
Utils::StringUnorderedMap<ShaderCache::FileNode> ShaderCache::s_FileGraph = {};
Utils::StringUnorderedMap<ShaderCache::Record> ShaderCache::s_Records = {};    
Utils::StringUnorderedMap<Shader*> ShaderCache::s_ShadersMap = {};    
std::vector<std::unique_ptr<Shader>> ShaderCache::s_Shaders = {};
std::vector<ShaderCache::PipelineData> ShaderCache::s_Pipelines = {};
Utils::StringUnorderedMap<ShaderDescriptors> ShaderCache::s_BindlessDescriptors = {};
DeletionQueue* ShaderCache::s_FrameDeletionQueue = {};

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
        Watcher.removeWatch(*CVars::Get().GetStringCVar({"Path.Shaders.Full"}));
    }
};
std::unique_ptr<ShaderCache::FileWatcher> ShaderCache::s_FileWatcher = {};

Shader::Shader(u32 pipelineIndex,
    const std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS>& descriptors)
        : m_Pipeline(pipelineIndex), m_Descriptors(descriptors)
{
}

const ShaderPipeline& Shader::Pipeline() const
{
    return ShaderCache::s_Pipelines[m_Pipeline].Pipeline;
}

ShaderOverrides Shader::CopyOverrides() const
{
    return ShaderOverrides{ShaderCache::s_Pipelines[m_Pipeline].OverrideHash};
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
        Pipeline::Destroy(s->Pipeline().GetPipeline());
    }
    s_FileWatcher.reset();
}

void ShaderCache::OnFrameBegin(FrameContext& ctx)
{
    s_FrameDeletionQueue = &ctx.DeletionQueue;
}

void ShaderCache::AddBindlessDescriptors(std::string_view name, const ShaderDescriptors& descriptors)
{
    s_BindlessDescriptors[std::string{name}] = descriptors;
}

const Shader& ShaderCache::Get(std::string_view name)
{
    return *s_ShadersMap.find(name)->second;
}

const Shader& ShaderCache::Register(std::string_view name, std::string_view path, const ShaderOverrides& overrides)
{
    ShaderProxy shaderProxy = {};
    u32 pipeline = {};
    
    if (!s_Records.contains(path))
    {
        /* if this is completely new shader */
        pipeline = (u32)s_Pipelines.size();
        shaderProxy = ReloadShader(path, ReloadType::PipelineDescriptors, overrides);
        s_Pipelines.push_back({.Pipeline = shaderProxy.Pipeline, .OverrideHash = overrides.m_Hash});
    }
    else
    {
        auto& shaders = s_Records.find(path)->second.Shaders;
        
        return Register(name, shaders.front(), overrides);
    }   

    return AddShader(name, pipeline, shaderProxy, path);
}

const Shader& ShaderCache::Register(std::string_view name, const Shader* shader, const ShaderOverrides& overrides)
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

    /* 2a) */
    if (s_ShadersMap.contains(name))
    {
        if (s_Pipelines[s_ShadersMap.find(name)->second->m_Pipeline].OverrideHash == overrides.m_Hash)
            return *s_ShadersMap.find(name)->second;
            
        s_Pipelines[s_ShadersMap.find(name)->second->m_Pipeline] = {
            .Pipeline = ReloadShader(shader->m_FilePath, ReloadType::Pipeline, overrides).Pipeline,
            .OverrideHash = overrides.m_Hash};
            
        return *s_ShadersMap.find(name)->second;
    }

    ShaderProxy shaderProxy = {};
    u32 pipeline = {};

    /* 1) */
    if (s_Pipelines[shader->m_Pipeline].OverrideHash == overrides.m_Hash)
    {
        pipeline = shader->m_Pipeline;
        shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Descriptors, overrides);
    }
    /* 2b) */
    else
    {
        pipeline = (u32)s_Pipelines.size();
        shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::PipelineDescriptors, overrides);
        s_Pipelines.push_back({.Pipeline = shaderProxy.Pipeline, .OverrideHash = overrides.m_Hash});
    }

    return AddShader(name, pipeline, shaderProxy, shader->m_FilePath);
}

const Shader& ShaderCache::AddShader(std::string_view name, u32 pipeline, const ShaderProxy& proxy, std::string_view path)
{
    s_Shaders.push_back(std::make_unique<Shader>(pipeline, proxy.Descriptors));
    s_Shaders.back()->m_FilePath = path;
    s_Shaders.back()->m_Features = proxy.Features;

    s_ShadersMap.emplace(name, s_Shaders.back().get());
    for (auto& dependency : proxy.Dependencies)
        s_Records[dependency].Shaders.push_back(s_Shaders.back().get());

    return *s_Shaders.back();
}

void ShaderCache::HandleRename(std::string_view newName, std::string_view oldName)
{
    if (s_Records.find(oldName)->second.Shaders.front()->m_FilePath == oldName)
        s_Records.find(oldName)->second.Shaders.front()->m_FilePath = newName;
    auto records = s_Records.extract(oldName);
    records.key() = newName;
    s_Records.insert(std::move(records));
}

void ShaderCache::HandleShaderModification(std::string_view name)
{
    auto it = s_Records.find(name);
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
        
        /* when the `ShaderOverride` has non-zero hash, it means that there are some overloads,
         * so this Reload operation will be useless, as we will have to reload it once more
         * with correct overloads; so instead we simply set pipeline overload hash to zero, thus
         * triggering reload on the next access operation
         */
        if (s_Pipelines[pipelineIndex].OverrideHash != 0)
        {
            s_Pipelines[pipelineIndex].OverrideHash = 0;
            continue;
        }
        
        ShaderProxy shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Pipeline,
            ShaderOverrides{0});
        s_Pipelines[pipelineIndex].Pipeline = shaderProxy.Pipeline;
    }
}

void ShaderCache::HandleStageModification(std::string_view name)
{
    auto& stages = s_FileGraph.find(name)->second.Files;
    ASSERT(stages.size() == 1, "Only .glsl files are meant to be used as includes")
    
    auto baked = ShaderStageConverter::Bake(*CVars::Get().GetStringCVar({"Path.Shaders.Full"}), name);
    if (baked.has_value())
        HandleShaderModification(stages.front().Processed);
}

void ShaderCache::HandleHeaderModification(std::string_view name)
{
    auto& stages = s_FileGraph.find(name)->second.Files;
    for (auto& stage : stages)
    {
        auto baked = ShaderStageConverter::Bake(*CVars::Get().GetStringCVar({"Path.Shaders.Full"}), stage.Raw);
        if (baked.has_value())
            HandleShaderModification(stage.Processed);
    }
}

void ShaderCache::CreateFileGraph()
{
    for (auto& file : std::filesystem::recursive_directory_iterator(*CVars::Get().GetStringCVar({"Path.Shaders.Full"})))
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
    const ShaderOverrides& overrides)
{
    std::ifstream in(path.data());
    nlohmann::json json = nlohmann::json::parse(in);

    const std::string& name = json["name"];

    std::vector<std::string_view> stages;
    stages.reserve(json["shader_stages"].size());
    for (auto& stage : json["shader_stages"])
        stages.push_back(stage);

    AssetManager::RemoveShader(AssetManager::GetShaderKey(stages));
    ShaderPipelineTemplate* shaderTemplate =
        ShaderTemplateLibrary::ReloadShaderPipelineTemplate(stages, name, *s_Allocators);

    ShaderProxy shader = {};
    shader.Dependencies.reserve(stages.size() + 1);
    for (auto& stage : stages)
        shader.Dependencies.emplace_back(stage);
    shader.Dependencies.emplace_back(path);
    
    shader.Features = shaderTemplate->GetDrawFeatures();

    if (reloadType == ReloadType::PipelineDescriptors || reloadType == ReloadType::Pipeline)
    {
        ShaderPipeline::Builder pipelineBuilder = {};
        pipelineBuilder
            .UseDescriptorBuffer()
            .SetTemplate(shaderTemplate);
        
        for (auto& specialization : json["specialization_constants"])
        {
            static constexpr std::string_view TYPE_BOOL = "b32";
            static constexpr std::string_view TYPE_I32 = "i32";
            static constexpr std::string_view TYPE_U32 = "u32";
            static constexpr std::string_view TYPE_F32 = "f32";
            
            std::string_view specializationName = specialization["name"];

            auto override = std::ranges::find_if(overrides.m_Overrides,
                [&specializationName](auto& o){ return o.Name.String() == specializationName; });
            if (override != overrides.m_Overrides.end())
            {
                // todo: std::visit ?
                ShaderOverrides::Override::ValueType value = override->Value;
                if (std::holds_alternative<bool>(value))
                    pipelineBuilder.AddSpecialization<bool>(override->Name.String(), std::get<bool>(value));
                else if (std::holds_alternative<i32>(value))
                    pipelineBuilder.AddSpecialization<i32>(override->Name.String(), std::get<i32>(value));
                else if (std::holds_alternative<u32>(value))
                    pipelineBuilder.AddSpecialization<u32>(override->Name.String(), std::get<u32>(value));
                else if (std::holds_alternative<f32>(value))
                    pipelineBuilder.AddSpecialization<f32>(override->Name.String(), std::get<f32>(value));
                else
                    LOG("Ignoring specialization constant {}: unknown type", override->Name.String());
            }
            else
            {
                std::string_view specializationType = specialization["type"];
                const auto& value = specialization["value"];
                if (specializationType == TYPE_BOOL)
                    pipelineBuilder.AddSpecialization<bool>(specializationName, value);
                else if (specializationType == TYPE_I32)
                    pipelineBuilder.AddSpecialization<i32>(specializationName, value);
                else if (specializationType == TYPE_U32)
                    pipelineBuilder.AddSpecialization<u32>(specializationName, value);
                else if (specializationType == TYPE_F32)
                    pipelineBuilder.AddSpecialization<f32>(specializationName, value);
                else
                    LOG("Ignoring specialization constant {}: unknown type: {}",
                        specializationName, specializationType);
            }
        }

        DynamicStates dynamicStates = DynamicStates::Default;
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
        pipelineBuilder.DynamicStates(dynamicStates);

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

            AlphaBlending blending = AlphaBlending::Over;
            DepthMode depthMode = DepthMode::ReadWrite;
            FaceCullMode cullMode = FaceCullMode::None;
            PrimitiveKind primitiveKind = PrimitiveKind::Triangle;
            bool depthClamp = false;

            if (rasterization.contains("alpha_blending"))
                blending = NAME_TO_BLENDING_MAP.at(rasterization["alpha_blending"]);
            if (rasterization.contains("depth_mode"))
                depthMode = NAME_TO_DEPTH_MODE_MAP.at(rasterization["depth_mode"]);
            if (rasterization.contains("cull_mode"))
                cullMode = NAME_TO_CULL_MODE_MAP.at(rasterization["cull_mode"]);
            if (rasterization.contains("primitive_kind"))
                primitiveKind = NAME_TO_PRIMITIVE_MAP.at(rasterization["primitive_kind"]);
            if (rasterization.contains("depth_clamp"))
                depthClamp = rasterization["depth_clamp"];

            std::vector<Format> colorFormats;
            std::optional<Format> depthFormat;

            colorFormats.reserve(rasterization["colors"].size());
            for (auto& color : rasterization["colors"])
                colorFormats.push_back(FormatUtils::formatFromString(color));
            if (rasterization.contains("depth"))
                depthFormat = FormatUtils::formatFromString(rasterization["depth"]);
            
            pipelineBuilder
                .AlphaBlending(blending)
                .DepthMode(depthMode)
                .DepthClamp(depthClamp)
                .FaceCullMode(cullMode)
                .PrimitiveKind(primitiveKind)
                .SetRenderingDetails({
                    .ColorFormats = colorFormats,
                    .DepthFormat = depthFormat ? *depthFormat : Format::Undefined});
        }
        
        shader.Pipeline = pipelineBuilder.BuildManualLifetime();
    }

    if (reloadType == ReloadType::Pipeline)
        return shader;
    /* basically there is no way to actually reload descriptors info
    * the descriptors filled by c++ code, and it is not really possible to make it work
    * w/o rewriting that code, so the descriptors are loaded once with the shader (no hot-reload for them)
    * changing the descriptors while application is running will lead to undefined behaviour
    */
    std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> descriptors = {};
    
    std::array<ShaderDescriptors::Builder, BINDLESS_DESCRIPTORS_INDEX> descriptorsBuilders = {};
    auto setPresence = shaderTemplate->GetSetPresence();
    for (u32 i = 0; i < BINDLESS_DESCRIPTORS_INDEX; i++)
    {
        if (!setPresence[i])
            continue;

        descriptorsBuilders[i]
            .SetTemplate(shaderTemplate, i == 0 ?
                DescriptorAllocatorKind::Samplers : DescriptorAllocatorKind::Resources)
            .ExtractSet(i);
    }

    for (u32 i = 0; i < BINDLESS_DESCRIPTORS_INDEX; i++)
        if (setPresence[i])
            shader.Descriptors[i] = descriptorsBuilders[i].Build();
    if (json.contains("bindless"))
    {
        shader.Descriptors[BINDLESS_DESCRIPTORS_INDEX] = s_BindlessDescriptors.at(json["bindless"]);
        // todo: this is pretty dangerous line, the templates might not be compatible
        shader.Descriptors[BINDLESS_DESCRIPTORS_INDEX].m_Template = shaderTemplate;
    }
    
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

            if (fileUpdateData.Action == efsw::Action::Moved)
            {
                ShaderCache::HandleRename(fileUpdateData.FileName, fileUpdateData.OldFileName);
                return;
            }

            ASSERT(fileUpdateData.Action == efsw::Action::Modified, "Unexpected file update action")
            std::string extension = filePath.extension().string();
            if (extension == SHADER_EXTENSION)
                ShaderCache::HandleShaderModification(fileUpdateData.FileName);
            else if (ShaderStageConverter::WatchesExtension(extension))
                ShaderCache::HandleStageModification(fileUpdateData.FileName);
            else if (extension == SHADER_HEADER_EXTENSION)
                ShaderCache::HandleHeaderModification(fileUpdateData.FileName);
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
    s_FileWatcher->Watcher.addWatch(*CVars::Get().GetStringCVar({"Path.Shaders.Full"}),
        s_FileWatcher->Listener.get(), IS_RECURSIVE);
    s_FileWatcher->Watcher.watch();
}
