#pragma once

#include "Rendering/Pipeline.h"
#include "String/StringId.h"

template <typename T>
struct ShaderSpecializationOverride
{
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