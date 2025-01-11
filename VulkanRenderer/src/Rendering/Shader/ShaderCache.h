#pragma once

#include "FrameContext.h"
#include "Shader.h"
#include "utils/HashedString.h"

#include <string>
#include <variant>

static constexpr u32 BINDLESS_DESCRIPTORS_INDEX = 2;
static_assert(BINDLESS_DESCRIPTORS_INDEX == 2, "Bindless descriptors are expected to be at index 2");

template <typename T>
struct ShaderOverride
{
    using Type = T;
    
    Utils::HashedString Name;
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
    std::array<Utils::HashedString, std::tuple_size_v<std::tuple<Args...>>> Names;
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
            Utils::hashCombine(
                Hash,
                std::get<Is>(tupleArgs).Name.Hash() ^
                Utils::hashBytes(
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
    Span<const Utils::HashedString> Names{};
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
    Shader(u32 pipelineIndex, const std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS>& descriptors);
    // todo: change to handles
    const Pipeline& Pipeline() const;
    const PipelineLayout& GetLayout() const;
    const ShaderDescriptors& Descriptors(ShaderDescriptorsKind kind) const { return m_Descriptors[(u32)kind]; }
    DrawFeatures Features() const { return m_Features; }

    ShaderOverridesView CopyOverrides() const;
private:
    u32 m_Pipeline{0};
    std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    DrawFeatures m_Features{};
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

    static void AddBindlessDescriptors(std::string_view name, const ShaderDescriptors& descriptors);
    
    /* returns shader associated with `name` */
    static const Shader& Get(std::string_view name);
    /* associates shader at `path` with `name` */
    static const Shader& Register(std::string_view name, std::string_view path,
        ShaderOverridesView&& overrides);
    /* associates shader with another `name` */
    static const Shader& Register(std::string_view name, const Shader* shader,
        ShaderOverridesView&& overrides);

private:
    static void HandleRename(std::string_view newName, std::string_view oldName);
    static void HandleShaderModification(std::string_view name);
    static void HandleStageModification(std::string_view name);
    static void HandleHeaderModification(std::string_view name);
    static void CreateFileGraph();
    struct ShaderProxy
    {
        Pipeline Pipeline;
        PipelineLayout PipelineLayout;
        std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> Descriptors;
        std::vector<std::string> Dependencies;
        DrawFeatures Features{};
    };
    static const Shader& AddShader(std::string_view name, u32 pipeline, const ShaderProxy& proxy,
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
    static Utils::StringUnorderedMap<FileNode> s_FileGraph;
    
    struct Record
    {
        std::vector<Shader*> Shaders; 
    };
    /* to achieve hot-reload we need to map each stage file (and its includes) to shaders */
    static Utils::StringUnorderedMap<Record> s_Records;

    /* maps associated name to shader */
    static Utils::StringUnorderedMap<Shader*> s_ShadersMap;
    
    static std::vector<std::unique_ptr<Shader>> s_Shaders;

    /* same shader file may produce different pipelines, based on used-provided overrides */
    struct PipelineData
    {
        Pipeline Pipeline;
        PipelineLayout PipelineLayout;
        u64 SpecializationsHash{0};
    };
    static std::vector<PipelineData> s_Pipelines;

    static Utils::StringUnorderedMap<ShaderDescriptors> s_BindlessDescriptors;

    static DeletionQueue* s_FrameDeletionQueue;

    struct FileWatcher;
    static std::unique_ptr<FileWatcher> s_FileWatcher;

    /* these arrays are filled by file watcher thread, but processed by the main thread to avoid race-conditions */
    static std::vector<std::pair<std::string, std::string>> s_ToRename;
    static std::vector<std::filesystem::path> s_ToReload;
};
