#pragma once

#include "Rendering/Pipeline.h"
#include "String/StringId.h"

#include <ranges>

template <typename T>
struct ShaderSpecialization
{
    using Type = T;
    
    StringId Name;
    T Value;

    static_assert(!std::is_pointer_v<T>);
    
    constexpr u32 SizeBytes() const
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

template <typename T>
concept ShaderSpecializationConcept = requires {
    requires std::same_as<T, ShaderSpecialization<typename T::Type>>;
};

template <typename ...Args>
struct ShaderSpecializations
{
    constexpr ShaderSpecializations(Args&&... args)
    requires (ShaderSpecializationConcept<Args> && ...)
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
                    sizeof(std::get<Is>(tupleArgs).Value))),
            Names[Is] = std::move(std::get<Is>(tupleArgs).Name),
            Descriptions[Is] = PipelineSpecializationDescription{
                .SizeBytes = (u32)std::get<Is>(tupleArgs).SizeBytes(),
                .Offset = (u32)offset},
            std::get<Is>(tupleArgs).CopyTo(Data.data() + offset), offset += std::get<Is>(tupleArgs).SizeBytes()),
            ...);
    }
};

struct ShaderDynamicSpecializations
{
    std::vector<std::byte> Data;
    std::vector<StringId> Names;
    std::vector<PipelineSpecializationDescription> Descriptions;
    u64 Hash{0};

    constexpr ShaderDynamicSpecializations() = default;
    template <typename ...Args>
    requires (ShaderSpecializationConcept<Args> && ...)
    constexpr ShaderDynamicSpecializations(Args&&... args)
    {
        CopyDataToVector(std::index_sequence_for<Args...>{}, std::tuple(std::forward<Args>(args)...));
    }

    template <typename T>
    ShaderDynamicSpecializations& Add(const ShaderSpecialization<T>& specialization);
private:
    template <std::size_t... Is, typename ...Args>
    constexpr void CopyDataToVector(std::index_sequence<Is...> seq, std::tuple<Args...>&& tupleArgs)
    {
        Data.resize((std::get<Is>(std::tuple<Args...>{}).SizeBytes() + ...));
        Names.resize(seq.size());
        Descriptions.resize(seq.size());
        u32 offset = 0;
        ((
            Hash::combine(
                Hash,
                std::get<Is>(tupleArgs).Name.Hash() ^
                Hash::bytes(
                    &std::get<Is>(tupleArgs).Value,
                    sizeof(std::get<Is>(tupleArgs).Value))),
            Names[Is] = std::move(std::get<Is>(tupleArgs).Name),
            Descriptions[Is] = PipelineSpecializationDescription{
                .SizeBytes = std::get<Is>(tupleArgs).SizeBytes(),
                .Offset = offset},
            std::get<Is>(tupleArgs).CopyTo(Data.data() + offset), offset += std::get<Is>(tupleArgs).SizeBytes()),
            ...);
    }
};

template <typename T>
ShaderDynamicSpecializations& ShaderDynamicSpecializations::Add(const ShaderSpecialization<T>& specialization)
{
    Hash::combine(Hash, specialization.Name.Hash() ^ Hash::bytes(&specialization.Value, sizeof(specialization.Value)));
    const u32 offset = (u32)Data.size();
    Names.push_back(specialization.Name);
    Descriptions.push_back(PipelineSpecializationDescription{
        .SizeBytes = specialization.SizeBytes(),
        .Offset = offset});
    Data.resize(offset + specialization.SizeBytes());
    specialization.CopyTo(Data.data() + offset);

    return *this;
}

struct ShaderSpecializationsView
{
    Span<const std::byte> Data{};
    Span<const StringId> Names{};
    Span<PipelineSpecializationDescription> Descriptions{};
    u64 Hash{0};

    ShaderSpecializationsView() = default;
    template <typename ...Args>
    constexpr ShaderSpecializationsView(ShaderSpecializations<Args...>&& specializations)
        :
        Data(specializations.Data), Names(specializations.Names), Descriptions(specializations.Descriptions),
        Hash(specializations.Hash) {}
    constexpr ShaderSpecializationsView(ShaderDynamicSpecializations& specializations)
        :
        Data(specializations.Data), Names(specializations.Names), Descriptions(specializations.Descriptions),
        Hash(specializations.Hash) {}
    PipelineSpecializationsView ToPipelineSpecializationsView(ShaderPipelineTemplate& shaderTemplate);
};

struct ShaderDefine
{
    StringId Name{};
    std::string Value{};
    
    constexpr ShaderDefine() = default;
    constexpr ShaderDefine(StringId name) : Name(name) {}
    template <typename T>
    requires requires(T val)
    {
        { std::to_string(val) } -> std::same_as<std::string>;
    }
    constexpr ShaderDefine(StringId name, T&& value) : Name(name), Value(std::to_string(std::forward<T>(value))) {}
    template <typename T>
    constexpr ShaderDefine(StringId name, T&& value) : Name(name), Value(std::forward<T>(value)) {}
};

struct ShaderDefines
{
    std::vector<ShaderDefine> Defines;
    u64 Hash{0};

    constexpr ShaderDefines() = default;
    constexpr ShaderDefines(Span<const ShaderDefine> defines)
    {
        Defines.resize(defines.size());
        for (auto&& [i, define] : std::ranges::views::enumerate(defines))
        {
            Hash::combine(Hash, define.Name.Hash() ^ Hash::string(define.Value)),
            Defines[i] = std::move(define);      
        }
    }
    ShaderDefines& Add(const ShaderDefine& define)
    {
        Hash::combine(Hash, define.Name.Hash() ^ Hash::string(define.Value));
        Defines.push_back(define);

        return *this;
    }
};

struct ShaderDefinesView
{
    Span<const ShaderDefine> Defines{};
    u64 Hash{0};
    
    constexpr ShaderDefinesView() = default;
    constexpr ShaderDefinesView(const ShaderDefines& defines) : Defines(defines.Defines), Hash(defines.Hash) {}
};

struct ShaderPipelineOverrides
{
    std::optional<DynamicStates> DynamicStates{std::nullopt};
    std::optional<DepthMode> DepthMode{std::nullopt};
    std::optional<FaceCullMode> CullMode{std::nullopt};
    std::optional<AlphaBlending> AlphaBlending{std::nullopt};
    std::optional<PrimitiveKind> PrimitiveKind{std::nullopt};
    std::optional<bool> ClampDepth{std::nullopt};

    constexpr u64 Hash() const
    {
        /* assert that this structure has no holes */
        static_assert(sizeof(ShaderPipelineOverrides) == (
            sizeof(std::optional<::DynamicStates>{}) +
            sizeof(std::optional<::DepthMode>{}) +
            sizeof(std::optional<::FaceCullMode>{}) +
            sizeof(std::optional<::AlphaBlending>{}) +
            sizeof(std::optional<::PrimitiveKind>{}) +
            sizeof(std::optional<bool>{})));

        return Hash::charBytes((const char*)this, sizeof(ShaderPipelineOverrides));
    }
};

struct ShaderOverrides
{
    ShaderDynamicSpecializations Specializations{};
    ShaderDefines Defines{};
    ShaderPipelineOverrides PipelineOverrides{};
    u64 Hash{0};

    constexpr ShaderOverrides() = default;
    constexpr ShaderOverrides(ShaderDynamicSpecializations&& specializations,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(std::forward<ShaderDynamicSpecializations>(specializations)),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    constexpr ShaderOverrides(ShaderDefines&& defines,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Defines(std::forward<ShaderDefines>(defines)),
        PipelineOverrides(pipelineOverrides)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    constexpr ShaderOverrides(ShaderDynamicSpecializations&& specializations, ShaderDefines&& defines,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(std::forward<ShaderDynamicSpecializations>(specializations)),
        Defines(std::forward<ShaderDefines>(defines)),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
};

struct ShaderOverridesView
{
    ShaderSpecializationsView Specializations{};
    ShaderDefinesView Defines{};
    ShaderPipelineOverrides PipelineOverrides{};
    u64 Hash{0};

    constexpr ShaderOverridesView() = default;
    constexpr ShaderOverridesView(ShaderOverrides& overrides)
        :
        Specializations(overrides.Specializations),
        Defines(overrides.Defines),
        PipelineOverrides(overrides.PipelineOverrides),
        Hash(overrides.Hash)
    {}
    
    template <typename ...Args>
    constexpr ShaderOverridesView(ShaderSpecializations<Args...>&& specializations,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(std::forward<ShaderSpecializations<Args...>>(specializations)),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    constexpr ShaderOverridesView(ShaderDynamicSpecializations& specializations,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(specializations),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    constexpr ShaderOverridesView(ShaderDefines&& defines,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Defines(std::forward<ShaderDefines>(defines)),
        PipelineOverrides(pipelineOverrides)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    template <typename ...Args>
    constexpr ShaderOverridesView(ShaderSpecializations<Args...>&& specializations, ShaderDefines&& defines,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(std::forward<ShaderSpecializations<Args...>>(specializations)),
        Defines(std::forward<ShaderDefines>(defines)),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
    constexpr ShaderOverridesView(ShaderDynamicSpecializations& specializations, ShaderDefines&& defines,
        ShaderPipelineOverrides&& pipelineOverrides = {})
        :
        Specializations(specializations),
        Defines(std::forward<ShaderDefines>(defines)),
        PipelineOverrides(pipelineOverrides),
        Hash(Specializations.Hash)
    {
        Hash::combine(Hash, Defines.Hash);
        Hash::combine(Hash, pipelineOverrides.Hash());
    }
};