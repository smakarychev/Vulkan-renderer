#pragma once

#include <AssetBakerLib/Bakers/BakerContext.h>

namespace lux::bakers
{
struct ImportError : IoError
{
    enum class ImportErrorCode : u8
    {
        None,
        NotBaked
    };
    ImportErrorCode ImportSpecificError{ImportErrorCode::None};
};


template <typename T>
using ImportResult = Result<T, ImportError>;
}

namespace std
{
template <>
struct formatter<lux::bakers::ImportError::ImportErrorCode> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(lux::bakers::ImportError::ImportErrorCode code, format_context& ctx) const
    {
        switch (code)
        {
        case lux::bakers::ImportError::ImportErrorCode::None:
            return format_to(ctx.out(), "None");
        case lux::bakers::ImportError::ImportErrorCode::NotBaked:
            return format_to(ctx.out(), "NotBaked");
        default:
            return format_to(ctx.out(), "Unknown error");
        }
    }
};
template <>
struct formatter<lux::bakers::ImportError> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const lux::bakers::ImportError& error, format_context& ctx) const
    {
        if (error.ImportSpecificError != lux::bakers::ImportError::ImportErrorCode::None)
            return  format_to(ctx.out(), "{} ({})", error.Message, error.ImportSpecificError);
        
        return format_to(ctx.out(), "{} ({})", error.Message, error.Code);
    }
};
}