#pragma once

#include "types.h"

namespace Passes 
{
enum class Channel : u32
{
    R, G, B, A, One, Zero
};

struct ChannelComposition
{
    Channel R{::Passes::Channel::R};
    Channel G{::Passes::Channel::G};
    Channel B{::Passes::Channel::B};
    Channel A{::Passes::Channel::A};

    constexpr static ChannelComposition RComposition()
    {
        return {.R = Channel::R, .G = Channel::R, .B = Channel::R, .A = Channel::One };
    }
    constexpr static ChannelComposition GComposition()
    {
        return {.R = Channel::G, .G = Channel::G, .B = Channel::G, .A = Channel::One };
    }
    constexpr static ChannelComposition BComposition()
    {
        return {.R = Channel::B, .G = Channel::B, .B = Channel::B, .A = Channel::One};
    }
    constexpr static ChannelComposition RGComposition()
    {
        return {.R = Channel::R, .G = Channel::G, .B = Channel::Zero, .A = Channel::One};
    }
    constexpr static ChannelComposition RBComposition()
    {
        return {.R = Channel::R, .G = Channel::Zero, .B = Channel::B, .A = Channel::One};
    }
    constexpr static ChannelComposition GBComposition()
    {
        return {.R = Channel::Zero, .G = Channel::G, .B = Channel::B, .A = Channel::One};
    }
    constexpr static ChannelComposition RGBComposition()
    {
        return {.R = Channel::R, .G = Channel::G, .B = Channel::B, .A = Channel::One};
    }
    constexpr static ChannelComposition RGBAComposition()
    {
        return {};
    }
};
}
