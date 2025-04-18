#pragma once

#include "FrameContext.h"
#include "Shader.h"
#include "String/StringUnorderedMap.h"
#include "String/StringId.h"

#include "ShaderOverrides.h"

class Shader
{
    friend class ShaderCache;
public:
    Shader(u32 pipelineIndex, const std::array<::Descriptors, MAX_DESCRIPTOR_SETS>& descriptors);
    Pipeline Pipeline() const;
    PipelineLayout GetLayout() const;
    const ::Descriptors& Descriptors(DescriptorsKind kind) const { return m_Descriptors[(u32)kind]; }

    ShaderOverridesView CopyOverrides() const;
private:
    u32 m_Pipeline{0};
    std::array<::Descriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    std::string m_FilePath;
};

class ShaderCache
{
    friend class Shader;
    static constexpr std::string_view SHADER_EXTENSION = ".shader";
    static constexpr std::string_view SHADER_HEADER_EXTENSION = ".glsl";
public:
    static void Init();
    static void Shutdown();
    static void SetAllocators(DescriptorArenaAllocators& allocators) { s_Allocators = &allocators; }
    static void OnFrameBegin(FrameContext& ctx);

    static void AddBindlessDescriptors(StringId name, const Descriptors& descriptors);
    
    /* returns shader associated with `name` */
    static const Shader& Get(StringId name);
    /* associates shader at `path` with `name` */
    static const Shader& Register(StringId name, std::string_view path, ShaderOverridesView&& overrides);
    /* associates shader with another `name` */
    static const Shader& Register(StringId name, const Shader* shader, ShaderOverridesView&& overrides);

private:
    static void HandleRename(std::string_view newPath, std::string_view oldPath);
    static void HandleShaderModification(std::string_view path);
    static void HandleStageModification(std::string_view path);
    static void HandleHeaderModification(std::string_view path);
    static void CreateFileGraph();
    struct ShaderProxy
    {
        Pipeline Pipeline;
        PipelineLayout PipelineLayout;
        std::array<Descriptors, MAX_DESCRIPTOR_SETS> Descriptors;
        std::vector<std::string> Dependencies;
    };
    static const Shader& AddShader(StringId name, u32 pipeline, const ShaderProxy& proxy,
        std::string_view path);
    enum class ReloadType { PipelineDescriptors, Descriptors, Pipeline };
    static ShaderProxy ReloadShader(std::string_view path, ReloadType reloadType,
        ShaderOverridesView&& overrides);
    static void InitFileWatcher();
private:
    static DescriptorArenaAllocators* s_Allocators;

    /* each .glsl, .vert, .frag, etc. has a list of other files that depend on it */
    struct FileNode
    {
        struct ShaderFile
        {
            std::string Raw;
            std::string Processed;
        };
        std::vector<ShaderFile> Files;
    };
    static StringUnorderedMap<FileNode> s_FileGraph;
    
    struct Record
    {
        std::vector<Shader*> Shaders; 
    };
    /* to achieve hot-reload we need to map each stage file (and its includes) to shaders */
    static StringUnorderedMap<Record> s_Records;

    /* maps associated name to shader */
    static std::unordered_map<StringId, Shader*> s_ShadersMap;
    
    static std::vector<std::unique_ptr<Shader>> s_Shaders;

    /* same shader file may produce different pipelines, based on used-provided overrides */
    struct PipelineData
    {
        Pipeline Pipeline;
        PipelineLayout PipelineLayout;
        u64 SpecializationsHash{0};
    };
    static std::vector<PipelineData> s_Pipelines;

    static std::unordered_map<StringId, Descriptors> s_BindlessDescriptors;

    static DeletionQueue* s_FrameDeletionQueue;

    struct FileWatcher;
    static std::unique_ptr<FileWatcher> s_FileWatcher;

    /* these arrays are filled by file watcher thread, but processed by the main thread to avoid race-conditions */
    static std::vector<std::pair<std::string, std::string>> s_ToRename;
    static std::vector<std::filesystem::path> s_ToReload;
};
