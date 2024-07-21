#pragma once
#include <string>

#include "FrameContext.h"
#include "Shader.h"
#include "utils/StringHasher.h"

static constexpr u32 MAX_DESCRIPTOR_SETS = 3;
static constexpr u32 BINDLESS_DESCRIPTORS_INDEX = 2;
static_assert(MAX_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");
static_assert(BINDLESS_DESCRIPTORS_INDEX == 2, "Bindless descriptors are expected to be at index 2");

class Shader
{
    friend class ShaderCache;
public:
    Shader(u32 pipelineIndex, const std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS>& descriptors);
    const ShaderPipeline& Pipeline() const;
    const ShaderDescriptors& Descriptors(ShaderDescriptorsKind kind) const { return m_Descriptors[(u32)(kind)]; }
private:
    u32 m_Pipeline{0};
    std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    std::string m_FilePath;
};

class ShaderCache
{
    friend class Shader;
public:
    static void Init();
    static void Shutdown();
    static void SetAllocators(DescriptorArenaAllocators& allocators) { s_Allocators = &allocators; }
    static void OnFrameBegin(FrameContext& ctx);

    static void AddBindlessDescriptors(std::string_view name, const ShaderDescriptors& descriptors);
    
    /* returns shader associated with `name` */
    static const Shader& Get(std::string_view name);
    /* associates shader at `path` with `name` */
    static void Register(std::string_view name, std::string_view path);

    static void HandleRename(std::string_view newName, std::string_view oldName);
    static void HandleModification(std::string_view path);
private:
    struct ShaderProxy
    {
        ShaderPipeline Pipeline;
        std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> Descriptors;
        std::vector<std::string> Dependencies;
    };
    enum class ReloadType { PipelineDescriptors, Descriptors, Pipeline };
    static ShaderProxy ReloadShader(std::string_view path, ReloadType reloadType);
    static void InitFileWatcher();
private:
    static DescriptorArenaAllocators* s_Allocators;
    
    struct Record
    {
        std::vector<Shader*> Shaders; 
    };
    /* to achieve hot-reload we need to map each stage file (and its includes) to shaders */
    static Utils::StringUnorderedMap<Record> s_Records;

    /* maps associated name to shader */
    static Utils::StringUnorderedMap<Shader*> s_ShadersMap;
    
    static std::vector<std::unique_ptr<Shader>> s_Shaders;
    static std::vector<ShaderPipeline> s_Pipelines;

    static Utils::StringUnorderedMap<ShaderDescriptors> s_BindlessDescriptors;

    static DeletionQueue* s_FrameDeletionQueue;

    struct FileWatcher;
    static std::unique_ptr<FileWatcher> s_FileWatcher;
};
