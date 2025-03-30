#include "FormatTraits.h"

#include <unordered_map>

#include "utils/HashedStringView.h"

namespace FormatUtils
{
    std::string formatToString(Format format)
    {
        switch (format)
        {
        case Format::Undefined:         return "Undefined";
        case Format::R8_UNORM:          return "R8_UNORM";
        case Format::R8_SNORM:          return "R8_SNORM";
        case Format::R8_UINT:           return "R8_UINT";
        case Format::R8_SINT:           return "R8_SINT";
        case Format::R8_SRGB:           return "R8_SRGB";
        case Format::RG8_UNORM:         return "RG8_UNORM";
        case Format::RG8_SNORM:         return "RG8_SNORM";
        case Format::RG8_UINT:          return "RG8_UINT";
        case Format::RG8_SINT:          return "RG8_SINT";
        case Format::RG8_SRGB:          return "RG8_SRGB";
        case Format::RGBA8_UNORM:       return "RGBA8_UNORM";
        case Format::RGBA8_SNORM:       return "RGBA8_SNORM";
        case Format::RGBA8_UINT:        return "RGBA8_UINT";
        case Format::RGBA8_SINT:        return "RGBA8_SINT";
        case Format::RGBA8_SRGB:        return "RGBA8_SRGB";
        case Format::R16_UNORM:         return "R16_UNORM";
        case Format::R16_SNORM:         return "R16_SNORM";
        case Format::R16_UINT:          return "R16_UINT";
        case Format::R16_SINT:          return "R16_SINT";
        case Format::R16_FLOAT:         return "R16_FLOAT";
        case Format::RG16_UNORM:        return "RG16_UNORM";
        case Format::RG16_SNORM:        return "RG16_SNORM";
        case Format::RG16_UINT:         return "RG16_UINT";
        case Format::RG16_SINT:         return "RG16_SINT";
        case Format::RG16_FLOAT:        return "RG16_FLOAT";
        case Format::RGBA16_UNORM:      return "RGBA16_UNORM";
        case Format::RGBA16_SNORM:      return "RGBA16_SNORM";
        case Format::RGBA16_UINT:       return "RGBA16_UINT";
        case Format::RGBA16_SINT:       return "RGBA16_SINT";
        case Format::RGBA16_FLOAT:      return "RGBA16_FLOAT";
        case Format::R32_UINT:          return "R32_UINT";
        case Format::R32_SINT:          return "R32_SINT";
        case Format::R32_FLOAT:         return "R32_FLOAT";
        case Format::RG32_UINT:         return "RG32_UINT";
        case Format::RG32_SINT:         return "RG32_SINT";
        case Format::RG32_FLOAT:        return "RG32_FLOAT";
        case Format::RGB32_UINT:        return "RGB32_UINT";
        case Format::RGB32_SINT:        return "RGB32_SINT";
        case Format::RGB32_FLOAT:       return "RGB32_FLOAT";
        case Format::RGBA32_UINT:       return "RGBA32_UINT";
        case Format::RGBA32_SINT:       return "RGBA32_SINT";
        case Format::RGBA32_FLOAT:      return "RGBA32_FLOAT";
        case Format::RGB10A2:           return "RGB10A2";
        case Format::R11G11B10:         return "R11G11B10";
        case Format::D32_FLOAT:         return "D32_FLOAT";
        case Format::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
        case Format::D32_FLOAT_S8_UINT: return "D32_FLOAT_S8_UINT";
        default:                        return "";
        }
    }

    Format formatFromString(std::string_view format)
    {
        static const Utils::StringUnorderedMap<Format> STRING_TO_FORMAT = {
            std::make_pair(formatToString(Format::Undefined),           Format::Undefined),
            std::make_pair(formatToString(Format::R8_UNORM),            Format::R8_UNORM),
            std::make_pair(formatToString(Format::R8_SNORM),            Format::R8_SNORM),
            std::make_pair(formatToString(Format::R8_UINT),             Format::R8_UINT),
            std::make_pair(formatToString(Format::R8_SINT),             Format::R8_SINT),
            std::make_pair(formatToString(Format::R8_SRGB),             Format::R8_SRGB),
            std::make_pair(formatToString(Format::RG8_UNORM),           Format::RG8_UNORM),
            std::make_pair(formatToString(Format::RG8_SNORM),           Format::RG8_SNORM),
            std::make_pair(formatToString(Format::RG8_UINT),            Format::RG8_UINT),
            std::make_pair(formatToString(Format::RG8_SINT),            Format::RG8_SINT),
            std::make_pair(formatToString(Format::RG8_SRGB),            Format::RG8_SRGB),
            std::make_pair(formatToString(Format::RGBA8_UNORM),         Format::RGBA8_UNORM),
            std::make_pair(formatToString(Format::RGBA8_SNORM),         Format::RGBA8_SNORM),
            std::make_pair(formatToString(Format::RGBA8_UINT),          Format::RGBA8_UINT),
            std::make_pair(formatToString(Format::RGBA8_SINT),          Format::RGBA8_SINT),
            std::make_pair(formatToString(Format::RGBA8_SRGB),          Format::RGBA8_SRGB),
            std::make_pair(formatToString(Format::R16_UNORM),           Format::R16_UNORM),
            std::make_pair(formatToString(Format::R16_SNORM),           Format::R16_SNORM),
            std::make_pair(formatToString(Format::R16_UINT),            Format::R16_UINT),
            std::make_pair(formatToString(Format::R16_SINT),            Format::R16_SINT),
            std::make_pair(formatToString(Format::R16_FLOAT),           Format::R16_FLOAT),
            std::make_pair(formatToString(Format::RG16_UNORM),          Format::RG16_UNORM),
            std::make_pair(formatToString(Format::RG16_SNORM),          Format::RG16_SNORM),
            std::make_pair(formatToString(Format::RG16_UINT),           Format::RG16_UINT),
            std::make_pair(formatToString(Format::RG16_SINT),           Format::RG16_SINT),
            std::make_pair(formatToString(Format::RG16_FLOAT),          Format::RG16_FLOAT),
            std::make_pair(formatToString(Format::RGBA16_UNORM),        Format::RGBA16_UNORM),
            std::make_pair(formatToString(Format::RGBA16_SNORM),        Format::RGBA16_SNORM),
            std::make_pair(formatToString(Format::RGBA16_UINT),         Format::RGBA16_UINT),
            std::make_pair(formatToString(Format::RGBA16_SINT),         Format::RGBA16_SINT),
            std::make_pair(formatToString(Format::RGBA16_FLOAT),        Format::RGBA16_FLOAT),
            std::make_pair(formatToString(Format::R32_UINT),            Format::R32_UINT),
            std::make_pair(formatToString(Format::R32_SINT),            Format::R32_SINT),
            std::make_pair(formatToString(Format::R32_FLOAT),           Format::R32_FLOAT),
            std::make_pair(formatToString(Format::RG32_UINT),           Format::RG32_UINT),
            std::make_pair(formatToString(Format::RG32_SINT),           Format::RG32_SINT),
            std::make_pair(formatToString(Format::RG32_FLOAT),          Format::RG32_FLOAT),
            std::make_pair(formatToString(Format::RGB32_UINT),          Format::RGB32_UINT),
            std::make_pair(formatToString(Format::RGB32_SINT),          Format::RGB32_SINT),
            std::make_pair(formatToString(Format::RGB32_FLOAT),         Format::RGB32_FLOAT),
            std::make_pair(formatToString(Format::RGBA32_UINT),         Format::RGBA32_UINT),
            std::make_pair(formatToString(Format::RGBA32_SINT),         Format::RGBA32_SINT),
            std::make_pair(formatToString(Format::RGBA32_FLOAT),        Format::RGBA32_FLOAT),
            std::make_pair(formatToString(Format::RGB10A2),             Format::RGB10A2),
            std::make_pair(formatToString(Format::R11G11B10),           Format::R11G11B10),
            std::make_pair(formatToString(Format::D32_FLOAT),           Format::D32_FLOAT),
            std::make_pair(formatToString(Format::D24_UNORM_S8_UINT),   Format::D24_UNORM_S8_UINT),
            std::make_pair(formatToString(Format::D32_FLOAT_S8_UINT),   Format::D32_FLOAT_S8_UINT),
        };

        return STRING_TO_FORMAT.find(format)->second;
    }
}
