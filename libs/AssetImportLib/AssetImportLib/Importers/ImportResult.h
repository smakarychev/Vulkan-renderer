#pragma once

#include <AssetImportLib/Importers/ImportContext.h>

namespace lux::import
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
struct formatter<lux::import::ImportError::ImportErrorCode> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(lux::import::ImportError::ImportErrorCode code, format_context& ctx) const
    {
        switch (code)
        {
        case lux::import::ImportError::ImportErrorCode::None:
            return format_to(ctx.out(), "None");
        case lux::import::ImportError::ImportErrorCode::NotBaked:
            return format_to(ctx.out(), "NotBaked");
        default:
            return format_to(ctx.out(), "Unknown error");
        }
    }
};
template <>
struct formatter<lux::import::ImportError> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const lux::import::ImportError& error, format_context& ctx) const
    {
        if (error.ImportSpecificError != lux::import::ImportError::ImportErrorCode::None)
            return  format_to(ctx.out(), "{} ({})", error.Message, error.ImportSpecificError);
        
        return format_to(ctx.out(), "{} ({})", error.Message, error.Code);
    }
};
}