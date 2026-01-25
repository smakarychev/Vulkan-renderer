#pragma once

#include "types.h"
#include "Containers/Result.h"
#include "v2/AssetLibV2.h"

#include <format>

namespace lux::assetlib::io
{
constexpr u32 ASSET_CURRENT_VERSION = 1;

struct IoError
{
    enum class ErrorCode : u8
    {
        GeneralError,
        FailedToCreate,
        FailedToOpen,
        FailedToLoad,
        WrongFormat,
    };
    ErrorCode Code{ErrorCode::GeneralError};
    std::string Message;
};

template <typename T>
using IoResult = Result<T, IoError>;

IoResult<AssetFile> unpackBaseAssetHeaderFromBuffer(std::string_view buffer);
IoResult<AssetFile> unpackBaseAssetHeader(const std::filesystem::path& headerPath);
IoResult<std::string> getAssetFullHeaderString(const AssetFile& file);
IoResult<std::string> getAssetFullHeaderFormattedString(const AssetFile& file);
}

namespace std
{
template <>
struct formatter<lux::assetlib::io::IoError::ErrorCode> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(lux::assetlib::io::IoError::ErrorCode code, format_context& ctx) const
    {
        switch (code)
        {
        case lux::assetlib::io::IoError::ErrorCode::GeneralError:
            return format_to(ctx.out(), "GeneralError");
        case lux::assetlib::io::IoError::ErrorCode::FailedToCreate:
            return format_to(ctx.out(), "FailedToCreate");
        case lux::assetlib::io::IoError::ErrorCode::FailedToOpen:
            return format_to(ctx.out(), "FailedToOpen");
        case lux::assetlib::io::IoError::ErrorCode::FailedToLoad:
            return format_to(ctx.out(), "FailedToLoad");
        case lux::assetlib::io::IoError::ErrorCode::WrongFormat:
            return format_to(ctx.out(), "WrongFormat");
        default:
            return format_to(ctx.out(), "Unknown error");
        }
    }
};
template <>
struct formatter<lux::assetlib::io::IoError> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const lux::assetlib::io::IoError& error, format_context& ctx) const
    {
        return format_to(ctx.out(), "{} ({})", error.Message, error.Code);
    }
};
}

#define ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, ...) \
if (!(x)) { return std::unexpected(::lux::assetlib::io::IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }