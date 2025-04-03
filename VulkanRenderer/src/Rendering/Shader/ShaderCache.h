#pragma once

#include "FrameContext.h"
#include "Shader.h"
#include "String/StringUnorderedMap.h"
#include "String/StringId.h"

#include <string>
#include <variant>


template <typename T>
struct ShaderOverride
{
    using Type = T;
    
    StringId Name;
    T Value;

    static_assert(!std::is_pointer_v<T>);
    
    constexpr usize SizeBytes() const
    {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>)
            return sizeof(u32);
        return sizeof(T);
    }
    constexpr void CopyTo(std::byte* dest) const
    {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            const u32 boolValue = (u32)Value;
            std::memcpy(dest, &boolValue, sizeof(boolValue));
        }
        else
        {
            std::memcpy(dest, &Value, sizeof(Value));
        }
    }
};

template <typename ...Args>
struct ShaderOverrides
{
    constexpr ShaderOverrides(Args&&... args)
    {
        CopyDataToArray(std::index_sequence_for<Args...>{}, std::tuple(std::forward<Args>(args)...));
    }

    template <std::size_t... Is>
    static constexpr usize CalculateSizeBytes(std::index_sequence<Is...>)
    {
        return (std::get<Is>(std::tuple<Args...>{}).SizeBytes() + ...);
    }

    std::array<std::byte, CalculateSizeBytes(std::index_sequence_for<Args...>{})> Data;
    std::array<StringId, std::tuple_size_v<std::tuple<Args...>>> Names;
    /* Descriptions are partially empty until the template is loaded
     * having it here helps to avoid dynamic memory allocations
     */
    std::array<PipelineSpecializationDescription, std::tuple_size_v<std::tuple<Args...>>> Descriptions;
    u64 Hash{0};
private:
    template <std::size_t... Is>
    constexpr void CopyDataToArray(std::index_sequence<Is...>, std::tuple<Args...>&& tupleArgs)
    {
        usize offset = 0;
        ((
            Hash::combine(
                Hash,
                std::get<Is>(tupleArgs).Name.Hash() ^
                Hash::bytes(
                    &std::get<Is>(tupleArgs).Value,
                    sizeof(&std::get<Is>(tupleArgs).Value))),
            Names[Is] = std::move(std::get<Is>(tupleArgs).Name),
            Descriptions[Is] = PipelineSpecializationDescription{
                .SizeBytes = (u32)std::get<Is>(tupleArgs).SizeBytes(),
                .Offset = (u32)offset},
            std::get<Is>(tupleArgs).CopyTo(Data.data() + offset), offset += std::get<Is>(tupleArgs).SizeBytes()),
            ...);
    }
};

struct ShaderOverridesView
{
    Span<const std::byte> Data{};
    Span<const StringId> Names{};
    Span<PipelineSpecializationDescription> Descriptions{};
    u64 Hash{0};

    ShaderOverridesView() = default;
    template <typename ...Args>
    constexpr ShaderOverridesView(ShaderOverrides<Args...>&& overrides)
        : Data(overrides.Data), Names(overrides.Names), Descriptions(overrides.Descriptions), Hash(overrides.Hash) {}
    PipelineSpecializationsView ToPipelineSpecializationsView(ShaderPipelineTemplate& shaderTemplate);
};

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
    static const Shader& Register(StringId, const Shader* shader, ShaderOverridesView&& overrides);

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
