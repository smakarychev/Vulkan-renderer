#pragma once
#include "core.h"
#include "types.h"
#include "Utils/HashUtils.h"

#include <algorithm>
#include <array>
#include <format>

class Guid
{
    static constexpr u32 GUID_HEX_STRING_SIZE_BYTES = 36;
    static constexpr char BAD_GUID_STRING[GUID_HEX_STRING_SIZE_BYTES + 1] = "badbadbadbadbadbadbadbadbadbadbadbad";
public:
    static constexpr u32 GUID_SIZE_BYTES = 16;
public:
    explicit consteval Guid(std::string_view guid) : m_Guid(ParseGuidString(guid))
    {
        if (!IsValid())
            m_Guid = ParseGuidString(BAD_GUID_STRING);
    }
    static constexpr Guid FromString(const std::string& guid)
    {
        Guid toReturn;
        toReturn.m_Guid = ParseGuidString(guid);
        if (!toReturn.IsValid())
            toReturn.m_Guid = ParseGuidString(BAD_GUID_STRING);

        return toReturn;
    } 
    
    constexpr Guid() = default;
    ~Guid() = default;
    constexpr Guid(const Guid& other) = default;
    constexpr Guid& operator=(const Guid& other) = default;
    constexpr Guid(Guid&& other) noexcept = default;
    constexpr Guid& operator=(Guid&& other) noexcept = default;

    auto constexpr operator<=>(const Guid&) const = default;

    const std::array<char, GUID_SIZE_BYTES>& AsArray() const { return m_Guid; }
    std::string AsString() const
    {
        return std::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-"
                           "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            m_Guid[0], m_Guid[1], m_Guid[2], m_Guid[3], m_Guid[4], m_Guid[5], m_Guid[6], m_Guid[7],
            m_Guid[8], m_Guid[9], m_Guid[10], m_Guid[11], m_Guid[12], m_Guid[13], m_Guid[14], m_Guid[15]);
    }
private:
    static constexpr std::array<char, GUID_SIZE_BYTES> ParseGuidString(std::string_view guid)
    {
        u32 stringI = 0;

        std::array<char, GUID_SIZE_BYTES> parsed{};
        for (char& byte : parsed)
        {
            static constexpr auto LUT = []() consteval {
                std::array<char, std::numeric_limits<char>::max()> array{};
                array.fill(~0);
                array['0'] = 0x0; array['1'] = 0x1; array['2'] = 0x2;
                array['3'] = 0x3; array['4'] = 0x4; array['5'] = 0x5;
                array['6'] = 0x6; array['7'] = 0x7; array['8'] = 0x8;
                array['9'] = 0x9; array['a'] = 0xa; array['b'] = 0xb;
                array['c'] = 0xc; array['d'] = 0xd; array['e'] = 0xe;
                array['f'] = 0xf; array['A'] = 0xa; array['B'] = 0xb;
                array['C'] = 0xc; array['D'] = 0xd; array['E'] = 0xe;
                array['F'] = 0xf;

                return array;
            }();

            if (guid[stringI] == '-')
                stringI += 1;

            if (stringI + 1 >= guid.size())
            {
                parsed.fill(~0);
                return parsed;
            }

            const char c = LUT[guid[stringI++]];
            const char cNext = LUT[guid[stringI++]];

            if (c == ~0 || cNext == ~0)
                byte = ~0;
            else
                byte = (char)(c << 4 | cNext);
        }

        return parsed;
    }
    constexpr bool IsValid() const
    {
        return std::ranges::all_of(m_Guid, [](auto c){ return c != ~0; });
    }
private:
    std::array<char, GUID_SIZE_BYTES> m_Guid{};
};

consteval Guid operator""_guid(const char* string, size_t length) noexcept {
    return Guid(std::string_view(string, length));
}


namespace std
{
template <>
struct hash<Guid>
{
    usize operator()(const Guid guid) const noexcept
    {
        return Hash::charBytes(guid.AsArray().data(), Guid::GUID_SIZE_BYTES);
    }
};

template <>
struct formatter<Guid>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(Guid guid, format_context& ctx) const
    {
        return format_to(ctx.out(), "{}", guid.AsString());
    }
};
}
