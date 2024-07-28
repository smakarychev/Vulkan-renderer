#pragma once

#include <string>
#include <variant>

#include "FrameContext.h"
#include "Shader.h"
#include "utils/HashedString.h"

static constexpr u32 MAX_DESCRIPTOR_SETS = 3;
static constexpr u32 BINDLESS_DESCRIPTORS_INDEX = 2;
static_assert(MAX_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");
static_assert(BINDLESS_DESCRIPTORS_INDEX == 2, "Bindless descriptors are expected to be at index 2");

/* holder for `specialization constants` and `defines` overloads */
class ShaderOverrides
{
    friend class ShaderCache;
public:
    constexpr ShaderOverrides() = default;
    constexpr ShaderOverrides(u64 hash) : m_Hash(hash) {}
    constexpr ~ShaderOverrides() = default;
    /* a futile attempt to make sure it does not outlive the `name` */
    ShaderOverrides(const ShaderOverrides&) = delete;
    ShaderOverrides& operator=(const ShaderOverrides&) = delete;
    ShaderOverrides(ShaderOverrides&&) = delete;
    ShaderOverrides& operator=(ShaderOverrides&&) = delete;
    
    template <typename T>
    constexpr ShaderOverrides& Add(Utils::HashedString name, T val);
private:
    struct Override
    {
        Utils::HashedString Name;
        /* std::any is another possible option, but we really have a limited number of data types
         * and this helps to avoid dynamic allocations
         */
        using ValueType = std::variant<bool, i32, u32, f32>; 
        ValueType Value;
    };
    u64 m_Hash{0};
    std::vector<Override> m_Overrides;
};

template <typename T>
constexpr ShaderOverrides& ShaderOverrides::Add(Utils::HashedString name, T val)
{
    static_assert(std::is_constructible_v<Override::ValueType, T>);
    m_Overrides.push_back(Override{.Name = name, .Value = val});
    if constexpr(std::is_same_v<bool, T>)
        m_Hash ^= name.Hash() ^ Utils::hashBytes(&val, sizeof(bool));
    else
        m_Hash ^= name.Hash() ^ std::bit_cast<u32>(val);

    return *this;
}

class Shader
{
    friend class ShaderCache;
public:
    Shader(u32 pipelineIndex, const std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS>& descriptors);
    const ShaderPipeline& Pipeline() const;
    const ShaderDescriptors& Descriptors(ShaderDescriptorsKind kind) const { return m_Descriptors[(u32)kind]; }
    DrawFeatures Features() const { return m_Features; }

    ShaderOverrides CopyOverrides() const;
private:
    u32 m_Pipeline{0};
    std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> m_Descriptors;
    DrawFeatures m_Features{};
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
    static const Shader& Register(std::string_view name, std::string_view path, const ShaderOverrides& overrides);
    /* associates shader with another `name` */
    static const Shader& Register(std::string_view name, const Shader* shader, const ShaderOverrides& overrides);

    static void HandleRename(std::string_view newName, std::string_view oldName);
    static void HandleModification(std::string_view path);
private:
    struct ShaderProxy
    {
        ShaderPipeline Pipeline;
        std::array<ShaderDescriptors, MAX_DESCRIPTOR_SETS> Descriptors;
        std::vector<std::string> Dependencies;
        DrawFeatures Features{};
    };
    static const Shader& AddShader(std::string_view name, u32 pipeline, const ShaderProxy& proxy,
        std::string_view path);
    enum class ReloadType { PipelineDescriptors, Descriptors, Pipeline };
    static ShaderProxy ReloadShader(std::string_view path, ReloadType reloadType, const ShaderOverrides& overrides);
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

    /* same shader file may produce different pipelines, based on used-provided overrides */
    struct PipelineData
    {
        ShaderPipeline Pipeline;
        u64 OverrideHash{0};
    };
    static std::vector<PipelineData> s_Pipelines;

    static Utils::StringUnorderedMap<ShaderDescriptors> s_BindlessDescriptors;

    static DeletionQueue* s_FrameDeletionQueue;

    struct FileWatcher;
    static std::unique_ptr<FileWatcher> s_FileWatcher;
};
