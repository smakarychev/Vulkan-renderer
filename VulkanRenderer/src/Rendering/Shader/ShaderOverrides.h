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
            Names[Is] = std::move(std::get<Is>(tupleArgs).Name),
            Descriptions[Is] = PipelineSpecializationDescription{
                .SizeBytes = (u32)std::get<Is>(tupleArgs).SizeBytes(),
                .Offset = (u32)offset},
            std::get<Is>(tupleArgs).CopyTo(Data.data() + offset),
            Hash::combine(
                Hash,
                std::get<Is>(tupleArgs).Name.Hash() ^
                Hash::bytes(
                    Data.data() + offset,
                    std::get<Is>(tupleArgs).SizeBytes())),
            offset += std::get<Is>(tupleArgs).SizeBytes()),
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
            Names[Is] = std::move(std::get<Is>(tupleArgs).Name),
            Descriptions[Is] = PipelineSpecializationDescription{
                .SizeBytes = std::get<Is>(tupleArgs).SizeBytes(),
                .Offset = offset},
            std::get<Is>(tupleArgs).CopyTo(Data.data() + offset),
            Hash::combine(
                Hash,
                std::get<Is>(tupleArgs).Name.Hash() ^
                Hash::bytes(
                    Data.data() + offset,
                    std::get<Is>(tupleArgs).SizeBytes())),
            offset += std::get<Is>(tupleArgs).SizeBytes()),
            ...);
    }
};

template <typename T>
ShaderDynamicSpecializations& ShaderDynamicSpecializations::Add(const ShaderSpecialization<T>& specialization)
{
    const u32 offset = (u32)Data.size();
    Names.push_back(specialization.Name);
    Descriptions.push_back(PipelineSpecializationDescription{
        .SizeBytes = specialization.SizeBytes(),
        .Offset = offset});
    Data.resize(offset + specialization.SizeBytes());
    specialization.CopyTo(Data.data() + offset);
    Hash::combine(Hash, specialization.Name.Hash() ^ Hash::bytes(Data.data() + offset, specialization.SizeBytes()));

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
    constexpr ShaderOverrides OverrideBy(const ShaderOverrides& other) const
    {
        ShaderOverrides merged = *this;

        for (auto&& [i, spec] : std::ranges::views::enumerate(other.Specializations.Names))
        {
            auto it = std::ranges::find_if(merged.Specializations.Names,
                [&](auto name){ return name == spec; });
            if (it == merged.Specializations.Names.end())
            {
                merged.Specializations.Names.emplace_back(spec);
                auto& description = other.Specializations.Descriptions[i];
                merged.Specializations.Descriptions.push_back({
                    .SizeBytes = description.SizeBytes,
                    .Offset = (u32)merged.Specializations.Data.size()});
                merged.Specializations.Data.append_range(
                    std::span(other.Specializations.Data.data() + description.Offset, description.SizeBytes));                
                continue;
            }

            u32 index = u32(it - merged.Specializations.Names.begin());
            auto& thisDesc = merged.Specializations.Descriptions[index];
            auto& otherDesc = other.Specializations.Descriptions[i];
            ASSERT(thisDesc.SizeBytes == otherDesc.SizeBytes, "Unable to merge")
            std::memcpy(
                merged.Specializations.Data.data() + thisDesc.Offset,
                other.Specializations.Data.data() + otherDesc.Offset,
                otherDesc.SizeBytes);
        }
        if (!other.Specializations.Names.empty())
        {
            merged.Specializations.Hash = 0;
            for (u32 i = 0; i < merged.Specializations.Descriptions.size(); i++)
            {
                auto& desc = merged.Specializations.Descriptions[i];
                Hash::combine(
                    merged.Specializations.Hash,
                    merged.Specializations.Names[i].Hash() ^
                    Hash::bytes(merged.Specializations.Data.data() + desc.Offset, desc.SizeBytes));
            }
        }

        for (auto& define : other.Defines.Defines)
            merged.Defines.Defines.emplace_back(define);
        if (!other.Defines.Defines.empty())
        {
            merged.Defines.Hash = 0;
            for (auto& define : merged.Defines.Defines)
                Hash::combine(merged.Defines.Hash, define.Name.Hash() ^ Hash::string(define.Value));
        }

        merged.PipelineOverrides.DynamicStates = other.PipelineOverrides.DynamicStates.has_value() ?
            *other.PipelineOverrides.DynamicStates : merged.PipelineOverrides.DynamicStates;
        merged.PipelineOverrides.DepthMode = other.PipelineOverrides.DepthMode.has_value() ?
            *other.PipelineOverrides.DepthMode : merged.PipelineOverrides.DepthMode;
        merged.PipelineOverrides.CullMode = other.PipelineOverrides.CullMode.has_value() ?
            *other.PipelineOverrides.CullMode : merged.PipelineOverrides.CullMode;
        merged.PipelineOverrides.AlphaBlending = other.PipelineOverrides.AlphaBlending.has_value() ?
            *other.PipelineOverrides.AlphaBlending : merged.PipelineOverrides.AlphaBlending;
        merged.PipelineOverrides.PrimitiveKind = other.PipelineOverrides.PrimitiveKind.has_value() ?
            *other.PipelineOverrides.PrimitiveKind : merged.PipelineOverrides.PrimitiveKind;
        merged.PipelineOverrides.ClampDepth = other.PipelineOverrides.ClampDepth.has_value() ?
            *other.PipelineOverrides.ClampDepth : merged.PipelineOverrides.ClampDepth;

        merged.Hash = merged.Specializations.Hash;
        Hash::combine(merged.Hash, merged.Defines.Hash);
        Hash::combine(merged.Hash, merged.PipelineOverrides.Hash());

        return merged;
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