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

DescriptorArenaAllocators* ShaderCache::s_Allocators = {nullptr};    
Utils::StringUnorderedMap<ShaderCache::Record> ShaderCache::s_Records = {};    
Utils::StringUnorderedMap<Shader*> ShaderCache::s_ShadersMap = {};    
std::vector<std::unique_ptr<Shader>> ShaderCache::s_Shaders = {};
std::vector<ShaderPipeline> ShaderCache::s_Pipelines = {};
Utils::StringUnorderedMap<ShaderDescriptors> ShaderCache::s_BindlessDescriptors = {};
DeletionQueue* ShaderCache::s_FrameDeletionQueue = {};

struct ShaderCache::FileWatcher
{
    // todo: move me to cvars
    static constexpr std::string_view SHADERS_DIRECTORY = "../assets/shaders";
    
    efsw::FileWatcher Watcher;
    std::shared_ptr<efsw::FileWatchListener> Listener;

    FileWatcher() = default;
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = default;
    FileWatcher& operator=(FileWatcher&&) = default;
    ~FileWatcher()
    {
        Watcher.removeWatch(std::string{SHADERS_DIRECTORY});
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
    return ShaderCache::s_Pipelines[m_Pipeline];
}

void ShaderCache::Init()
{
    InitFileWatcher();
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

void ShaderCache::Register(std::string_view name, std::string_view path)
{
    if (s_ShadersMap.contains(name))
        return;

    ShaderProxy shaderProxy = {};
    u32 pipeline = {};
    if (!s_Records.contains(path))
    {
        shaderProxy = ReloadShader(path, ReloadType::PipelineDescriptors);
        pipeline = (u32)s_Pipelines.size();
        s_Pipelines.push_back(shaderProxy.Pipeline);
    }
    else
    {
        shaderProxy = ReloadShader(path, ReloadType::Descriptors);
        Shader* shader = s_Records.find(path)->second.Shaders.front();
        pipeline = shader->m_Pipeline;
    }

    s_Shaders.push_back(std::make_unique<Shader>(pipeline, shaderProxy.Descriptors));
    s_Shaders.back()->m_FilePath = path;

    s_ShadersMap.emplace(name, s_Shaders.back().get());
    for (auto& dependency : shaderProxy.Dependencies)
        s_Records[dependency].Shaders.push_back(s_Shaders.back().get());
}

void ShaderCache::HandleRename(std::string_view newName, std::string_view oldName)
{
    if (s_Records.find(oldName)->second.Shaders.front()->m_FilePath == oldName)
        s_Records.find(oldName)->second.Shaders.front()->m_FilePath = newName;
    auto records = s_Records.extract(oldName);
    records.key() = newName;
    s_Records.insert(std::move(records));
}

void ShaderCache::HandleModification(std::string_view path)
{
    const Record& record = s_Records.find(path)->second;
    std::unordered_set<Shader*> handled = {};
    for (auto* shader : record.Shaders)
    {
        if (handled.contains(shader))
            continue;
        handled.emplace(shader);

        ShaderProxy shaderProxy = ReloadShader(shader->m_FilePath, ReloadType::Pipeline);
        s_FrameDeletionQueue->Enqueue(s_Pipelines[shader->m_Pipeline]);
        s_Pipelines[shader->m_Pipeline] = shaderProxy.Pipeline;
    }
}

ShaderCache::ShaderProxy ShaderCache::ReloadShader(std::string_view path, ReloadType reloadType)
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
    shader.Dependencies.reserve(shaderTemplate->GetShaderDependencies().size() + 1);
    shader.Dependencies.append_range(shaderTemplate->GetShaderDependencies());
    shader.Dependencies.emplace_back(path);

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
                LOG("Ignoring specialization constant {}: unknown type: {}", specializationName, specializationType);
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
            std::string filePath = std::filesystem::path(directory + filename).generic_string();
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
                        .OldFileName = std::filesystem::path(directory + oldFilename).generic_string(),
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

            static constexpr milliseconds DEBOUNCE_TIME = 150ms;

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
            LOG("filename: {}", fileUpdateData.FileName);

            /* is it possible that file is deleted or renamed before we begin to process it */
            if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath))
                return;

            if (fileUpdateData.Action == efsw::Action::Moved)
            {
                ShaderCache::HandleRename(fileUpdateData.FileName, fileUpdateData.OldFileName);
                return;
            }

            ASSERT(fileUpdateData.Action == efsw::Action::Modified, "Unexpected file update action")
            
            static constexpr std::string_view SHADER_EXTENSION = ".shader";
            std::string extension = filePath.extension().string();
            if (ShaderStageConverter::WatchesExtension(extension))
            {
                std::optional<assetLib::ShaderStageInfo> stageInfo =
                    ShaderStageConverter::Bake(FileWatcher::SHADERS_DIRECTORY, filePath);
                if (!stageInfo.has_value())
                    return;

                ShaderCache::HandleModification(fileUpdateData.FileName);
            }
            else if (extension == SHADER_EXTENSION)
            {
                ShaderCache::HandleModification(fileUpdateData.FileName);
            }
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
    s_FileWatcher->Watcher.addWatch(std::string{FileWatcher::SHADERS_DIRECTORY},
        s_FileWatcher->Listener.get(), IS_RECURSIVE);
    s_FileWatcher->Watcher.watch();
}
