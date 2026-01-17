#include "rendererpch.h"

#include "FormatTraits.h"

#include "String/StringHeterogeneousHasher.h"

namespace FormatUtils
{
std::string formatToString(Format format)
{
    switch (format)
    {
    case Format::Undefined: return "Undefined";
    case Format::RG4_UNORM_PACK8: return "RG4_UNORM_PACK8";
    case Format::RGBA4_UNORM_PACK16: return "RGBA4_UNORM_PACK16";
    case Format::BGRA4_UNORM_PACK16: return "BGRA4_UNORM_PACK16";
    case Format::R5G6B5_UNORM_PACK16: return "R5G6B5_UNORM_PACK16";
    case Format::B5G6R5_UNORM_PACK16: return "B5G6R5_UNORM_PACK16";
    case Format::RGB5A1_UNORM_PACK16: return "RGB5A1_UNORM_PACK16";
    case Format::BGR5A1_UNORM_PACK16: return "BGR5A1_UNORM_PACK16";
    case Format::A1RGB5_UNORM_PACK16: return "A1RGB5_UNORM_PACK16";
    case Format::R8_UNORM: return "R8_UNORM";
    case Format::R8_SNORM: return "R8_SNORM";
    case Format::R8_USCALED: return "R8_USCALED";
    case Format::R8_SSCALED: return "R8_SSCALED";
    case Format::R8_UINT: return "R8_UINT";
    case Format::R8_SINT: return "R8_SINT";
    case Format::R8_SRGB: return "R8_SRGB";
    case Format::RG8_UNORM: return "RG8_UNORM";
    case Format::RG8_SNORM: return "RG8_SNORM";
    case Format::RG8_USCALED: return "RG8_USCALED";
    case Format::RG8_SSCALED: return "RG8_SSCALED";
    case Format::RG8_UINT: return "RG8_UINT";
    case Format::RG8_SINT: return "RG8_SINT";
    case Format::RG8_SRGB: return "RG8_SRGB";
    case Format::RGB8_UNORM: return "RGB8_UNORM";
    case Format::RGB8_SNORM: return "RGB8_SNORM";
    case Format::RGB8_USCALED: return "RGB8_USCALED";
    case Format::RGB8_SSCALED: return "RGB8_SSCALED";
    case Format::RGB8_UINT: return "RGB8_UINT";
    case Format::RGB8_SINT: return "RGB8_SINT";
    case Format::RGB8_SRGB: return "RGB8_SRGB";
    case Format::BGR8_UNORM: return "BGR8_UNORM";
    case Format::BGR8_SNORM: return "BGR8_SNORM";
    case Format::BGR8_USCALED: return "BGR8_USCALED";
    case Format::BGR8_SSCALED: return "BGR8_SSCALED";
    case Format::BGR8_UINT: return "BGR8_UINT";
    case Format::BGR8_SINT: return "BGR8_SINT";
    case Format::BGR8_SRGB: return "BGR8_SRGB";
    case Format::RGBA8_UNORM: return "RGBA8_UNORM";
    case Format::RGBA8_SNORM: return "RGBA8_SNORM";
    case Format::RGBA8_USCALED: return "RGBA8_USCALED";
    case Format::RGBA8_SSCALED: return "RGBA8_SSCALED";
    case Format::RGBA8_UINT: return "RGBA8_UINT";
    case Format::RGBA8_SINT: return "RGBA8_SINT";
    case Format::RGBA8_SRGB: return "RGBA8_SRGB";
    case Format::BGRA8_UNORM: return "BGRA8_UNORM";
    case Format::BGRA8_SNORM: return "BGRA8_SNORM";
    case Format::BGRA8_USCALED: return "BGRA8_USCALED";
    case Format::BGRA8_SSCALED: return "BGRA8_SSCALED";
    case Format::BGRA8_UINT: return "BGRA8_UINT";
    case Format::BGRA8_SINT: return "BGRA8_SINT";
    case Format::BGRA8_SRGB: return "BGRA8_SRGB";
    case Format::ABGR8_UNORM_PACK32: return "ABGR8_UNORM_PACK32";
    case Format::ABGR8_SNORM_PACK32: return "ABGR8_SNORM_PACK32";
    case Format::ABGR8_USCALED_PACK32: return "ABGR8_USCALED_PACK32";
    case Format::ABGR8_SSCALED_PACK32: return "ABGR8_SSCALED_PACK32";
    case Format::ABGR8_UINT_PACK32: return "ABGR8_UINT_PACK32";
    case Format::ABGR8_SINT_PACK32: return "ABGR8_SINT_PACK32";
    case Format::ABGR8_SRGB_PACK32: return "ABGR8_SRGB_PACK32";
    case Format::A2RGB10_UNORM_PACK32: return "A2RGB10_UNORM_PACK32";
    case Format::A2RGB10_SNORM_PACK32: return "A2RGB10_SNORM_PACK32";
    case Format::A2RGB10_USCALED_PACK32: return "A2RGB10_USCALED_PACK32";
    case Format::A2RGB10_SSCALED_PACK32: return "A2RGB10_SSCALED_PACK32";
    case Format::A2RGB10_UINT_PACK32: return "A2RGB10_UINT_PACK32";
    case Format::A2RGB10_SINT_PACK32: return "A2RGB10_SINT_PACK32";
    case Format::A2BGR10_UNORM_PACK32: return "A2BGR10_UNORM_PACK32";
    case Format::A2BGR10_SNORM_PACK32: return "A2BGR10_SNORM_PACK32";
    case Format::A2BGR10_USCALED_PACK32: return "A2BGR10_USCALED_PACK32";
    case Format::A2BGR10_SSCALED_PACK32: return "A2BGR10_SSCALED_PACK32";
    case Format::A2BGR10_UINT_PACK32: return "A2BGR10_UINT_PACK32";
    case Format::A2BGR10_SINT_PACK32: return "A2BGR10_SINT_PACK32";
    case Format::R16_UNORM: return "R16_UNORM";
    case Format::R16_SNORM: return "R16_SNORM";
    case Format::R16_USCALED: return "R16_USCALED";
    case Format::R16_SSCALED: return "R16_SSCALED";
    case Format::R16_UINT: return "R16_UINT";
    case Format::R16_SINT: return "R16_SINT";
    case Format::R16_FLOAT: return "R16_FLOAT";
    case Format::RG16_UNORM: return "RG16_UNORM";
    case Format::RG16_SNORM: return "RG16_SNORM";
    case Format::RG16_USCALED: return "RG16_USCALED";
    case Format::RG16_SSCALED: return "RG16_SSCALED";
    case Format::RG16_UINT: return "RG16_UINT";
    case Format::RG16_SINT: return "RG16_SINT";
    case Format::RG16_FLOAT: return "RG16_FLOAT";
    case Format::RGB16_UNORM: return "RGB16_UNORM";
    case Format::RGB16_SNORM: return "RGB16_SNORM";
    case Format::RGB16_USCALED: return "RGB16_USCALED";
    case Format::RGB16_SSCALED: return "RGB16_SSCALED";
    case Format::RGB16_UINT: return "RGB16_UINT";
    case Format::RGB16_SINT: return "RGB16_SINT";
    case Format::RGB16_FLOAT: return "RGB16_FLOAT";
    case Format::RGBA16_UNORM: return "RGBA16_UNORM";
    case Format::RGBA16_SNORM: return "RGBA16_SNORM";
    case Format::RGBA16_USCALED: return "RGBA16_USCALED";
    case Format::RGBA16_SSCALED: return "RGBA16_SSCALED";
    case Format::RGBA16_UINT: return "RGBA16_UINT";
    case Format::RGBA16_SINT: return "RGBA16_SINT";
    case Format::RGBA16_FLOAT: return "RGBA16_FLOAT";
    case Format::R32_UINT: return "R32_UINT";
    case Format::R32_SINT: return "R32_SINT";
    case Format::R32_FLOAT: return "R32_FLOAT";
    case Format::RG32_UINT: return "RG32_UINT";
    case Format::RG32_SINT: return "RG32_SINT";
    case Format::RG32_FLOAT: return "RG32_FLOAT";
    case Format::RGB32_UINT: return "RGB32_UINT";
    case Format::RGB32_SINT: return "RGB32_SINT";
    case Format::RGB32_FLOAT: return "RGB32_FLOAT";
    case Format::RGBA32_UINT: return "RGBA32_UINT";
    case Format::RGBA32_SINT: return "RGBA32_SINT";
    case Format::RGBA32_FLOAT: return "RGBA32_FLOAT";
    case Format::R64_UINT: return "R64_UINT";
    case Format::R64_SINT: return "R64_SINT";
    case Format::R64_FLOAT: return "R64_FLOAT";
    case Format::RG64_UINT: return "RG64_UINT";
    case Format::RG64_SINT: return "RG64_SINT";
    case Format::RG64_FLOAT: return "RG64_FLOAT";
    case Format::RGB64_UINT: return "RGB64_UINT";
    case Format::RGB64_SINT: return "RGB64_SINT";
    case Format::RGB64_FLOAT: return "RGB64_FLOAT";
    case Format::RGBA64_UINT: return "RGBA64_UINT";
    case Format::RGBA64_SINT: return "RGBA64_SINT";
    case Format::RGBA64_FLOAT: return "RGBA64_FLOAT";
    case Format::B10G11R11_UFLOAT_PACK32: return "B10G11R11_UFLOAT_PACK32";
    case Format::E5BGR9_UFLOAT_PACK32: return "E5BGR9_UFLOAT_PACK32";
    case Format::D16_UNORM: return "D16_UNORM";
    case Format::X8_D24_UNORM_PACK32: return "X8_D24_UNORM_PACK32";
    case Format::D32_FLOAT: return "D32_FLOAT";
    case Format::S8_UINT: return "S8_UINT";
    case Format::D16_UNORM_S8_UINT: return "D16_UNORM_S8_UINT";
    case Format::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
    case Format::D32_FLOAT_S8_UINT: return "D32_FLOAT_S8_UINT";
    case Format::BC1_RGB_UNORM_BLOCK: return "BC1_RGB_UNORM_BLOCK";
    case Format::BC1_RGB_SRGB_BLOCK: return "BC1_RGB_SRGB_BLOCK";
    case Format::BC1_RGBA_UNORM_BLOCK: return "BC1_RGBA_UNORM_BLOCK";
    case Format::BC1_RGBA_SRGB_BLOCK: return "BC1_RGBA_SRGB_BLOCK";
    case Format::BC2_UNORM_BLOCK: return "BC2_UNORM_BLOCK";
    case Format::BC2_SRGB_BLOCK: return "BC2_SRGB_BLOCK";
    case Format::BC3_UNORM_BLOCK: return "BC3_UNORM_BLOCK";
    case Format::BC3_SRGB_BLOCK: return "BC3_SRGB_BLOCK";
    case Format::BC4_UNORM_BLOCK: return "BC4_UNORM_BLOCK";
    case Format::BC4_SNORM_BLOCK: return "BC4_SNORM_BLOCK";
    case Format::BC5_UNORM_BLOCK: return "BC5_UNORM_BLOCK";
    case Format::BC5_SNORM_BLOCK: return "BC5_SNORM_BLOCK";
    case Format::BC6H_UFLOAT_BLOCK: return "BC6H_UFLOAT_BLOCK";
    case Format::BC6H_FLOAT_BLOCK: return "BC6H_FLOAT_BLOCK";
    case Format::BC7_UNORM_BLOCK: return "BC7_UNORM_BLOCK";
    case Format::BC7_SRGB_BLOCK: return "BC7_SRGB_BLOCK";
    case Format::ETC2_RGB8_UNORM_BLOCK: return "ETC2_RGB8_UNORM_BLOCK";
    case Format::ETC2_RGB8_SRGB_BLOCK: return "ETC2_RGB8_SRGB_BLOCK";
    case Format::ETC2_RGB8A1_UNORM_BLOCK: return "ETC2_RGB8A1_UNORM_BLOCK";
    case Format::ETC2_RGB8A1_SRGB_BLOCK: return "ETC2_RGB8A1_SRGB_BLOCK";
    case Format::ETC2_RGBA8_UNORM_BLOCK: return "ETC2_RGBA8_UNORM_BLOCK";
    case Format::ETC2_RGBA8_SRGB_BLOCK: return "ETC2_RGBA8_SRGB_BLOCK";
    case Format::EAC_R11_UNORM_BLOCK: return "EAC_R11_UNORM_BLOCK";
    case Format::EAC_R11_SNORM_BLOCK: return "EAC_R11_SNORM_BLOCK";
    case Format::EAC_R11G11_UNORM_BLOCK: return "EAC_R11G11_UNORM_BLOCK";
    case Format::EAC_R11G11_SNORM_BLOCK: return "EAC_R11G11_SNORM_BLOCK";
    case Format::ASTC_4x4_UNORM_BLOCK: return "ASTC_4x4_UNORM_BLOCK";
    case Format::ASTC_4x4_SRGB_BLOCK: return "ASTC_4x4_SRGB_BLOCK";
    case Format::ASTC_5x4_UNORM_BLOCK: return "ASTC_5x4_UNORM_BLOCK";
    case Format::ASTC_5x4_SRGB_BLOCK: return "ASTC_5x4_SRGB_BLOCK";
    case Format::ASTC_5x5_UNORM_BLOCK: return "ASTC_5x5_UNORM_BLOCK";
    case Format::ASTC_5x5_SRGB_BLOCK: return "ASTC_5x5_SRGB_BLOCK";
    case Format::ASTC_6x5_UNORM_BLOCK: return "ASTC_6x5_UNORM_BLOCK";
    case Format::ASTC_6x5_SRGB_BLOCK: return "ASTC_6x5_SRGB_BLOCK";
    case Format::ASTC_6x6_UNORM_BLOCK: return "ASTC_6x6_UNORM_BLOCK";
    case Format::ASTC_6x6_SRGB_BLOCK: return "ASTC_6x6_SRGB_BLOCK";
    case Format::ASTC_8x5_UNORM_BLOCK: return "ASTC_8x5_UNORM_BLOCK";
    case Format::ASTC_8x5_SRGB_BLOCK: return "ASTC_8x5_SRGB_BLOCK";
    case Format::ASTC_8x6_UNORM_BLOCK: return "ASTC_8x6_UNORM_BLOCK";
    case Format::ASTC_8x6_SRGB_BLOCK: return "ASTC_8x6_SRGB_BLOCK";
    case Format::ASTC_8x8_UNORM_BLOCK: return "ASTC_8x8_UNORM_BLOCK";
    case Format::ASTC_8x8_SRGB_BLOCK: return "ASTC_8x8_SRGB_BLOCK";
    case Format::ASTC_10x5_UNORM_BLOCK: return "ASTC_10x5_UNORM_BLOCK";
    case Format::ASTC_10x5_SRGB_BLOCK: return "ASTC_10x5_SRGB_BLOCK";
    case Format::ASTC_10x6_UNORM_BLOCK: return "ASTC_10x6_UNORM_BLOCK";
    case Format::ASTC_10x6_SRGB_BLOCK: return "ASTC_10x6_SRGB_BLOCK";
    case Format::ASTC_10x8_UNORM_BLOCK: return "ASTC_10x8_UNORM_BLOCK";
    case Format::ASTC_10x8_SRGB_BLOCK: return "ASTC_10x8_SRGB_BLOCK";
    case Format::ASTC_10x10_UNORM_BLOCK: return "ASTC_10x10_UNORM_BLOCK";
    case Format::ASTC_10x10_SRGB_BLOCK: return "ASTC_10x10_SRGB_BLOCK";
    case Format::ASTC_12x10_UNORM_BLOCK: return "ASTC_12x10_UNORM_BLOCK";
    case Format::ASTC_12x10_SRGB_BLOCK: return "ASTC_12x10_SRGB_BLOCK";
    case Format::ASTC_12x12_UNORM_BLOCK: return "ASTC_12x12_UNORM_BLOCK";
    case Format::ASTC_12x12_SRGB_BLOCK: return "ASTC_12x12_SRGB_BLOCK";
    case Format::GBGR8_422_UNORM: return "GBGR8_422_UNORM";
    case Format::B8G8RG8_422_UNORM: return "B8G8RG8_422_UNORM";
    case Format::G8_B8_R8_3PLANE_420_UNORM: return "G8_B8_R8_3PLANE_420_UNORM";
    case Format::G8_B8R8_2PLANE_420_UNORM: return "G8_B8R8_2PLANE_420_UNORM";
    case Format::G8_B8_R8_3PLANE_422_UNORM: return "G8_B8_R8_3PLANE_422_UNORM";
    case Format::G8_B8R8_2PLANE_422_UNORM: return "G8_B8R8_2PLANE_422_UNORM";
    case Format::G8_B8_R8_3PLANE_444_UNORM: return "G8_B8_R8_3PLANE_444_UNORM";
    case Format::R10X6_UNORM_PACK16: return "R10X6_UNORM_PACK16";
    case Format::R10X6G10X6_UNORM_2PACK16: return "R10X6G10X6_UNORM_2PACK16";
    case Format::R10X6G10X6B10X6A10X6_UNORM_4PACK16: return "R10X6G10X6B10X6A10X6_UNORM_4PACK16";
    case Format::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return "G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
    case Format::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return "B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
    case Format::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
    case Format::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
    case Format::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
    case Format::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
    case Format::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
    case Format::R12X4_UNORM_PACK16: return "R12X4_UNORM_PACK16";
    case Format::R12X4G12X4_UNORM_2PACK16: return "R12X4G12X4_UNORM_2PACK16";
    case Format::R12X4G12X4B12X4A12X4_UNORM_4PACK16: return "R12X4G12X4B12X4A12X4_UNORM_4PACK16";
    case Format::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return "G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
    case Format::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return "B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
    case Format::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
    case Format::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
    case Format::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
    case Format::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return "G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
    case Format::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
    case Format::G16B16G16R16_422_UNORM: return "G16B16G16R16_422_UNORM";
    case Format::B16G16RG16_422_UNORM: return "B16G16RG16_422_UNORM";
    case Format::G16_B16_R16_3PLANE_420_UNORM: return "G16_B16_R16_3PLANE_420_UNORM";
    case Format::G16_B16R16_2PLANE_420_UNORM: return "G16_B16R16_2PLANE_420_UNORM";
    case Format::G16_B16_R16_3PLANE_422_UNORM: return "G16_B16_R16_3PLANE_422_UNORM";
    case Format::G16_B16R16_2PLANE_422_UNORM: return "G16_B16R16_2PLANE_422_UNORM";
    case Format::G16_B16_R16_3PLANE_444_UNORM: return "G16_B16_R16_3PLANE_444_UNORM";
    case Format::G8_B8R8_2PLANE_444_UNORM: return "G8_B8R8_2PLANE_444_UNORM";
    case Format::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return "G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
    case Format::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return "G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
    case Format::G16_B16R16_2PLANE_444_UNORM: return "G16_B16R16_2PLANE_444_UNORM";
    case Format::A4RGB4_UNORM_PACK16: return "A4RGB4_UNORM_PACK16";
    case Format::A4B4G4R4_UNORM_PACK16: return "A4B4G4R4_UNORM_PACK16";
    case Format::ASTC_4x4_FLOAT_BLOCK: return "ASTC_4x4_FLOAT_BLOCK";
    case Format::ASTC_5x4_FLOAT_BLOCK: return "ASTC_5x4_FLOAT_BLOCK";
    case Format::ASTC_5x5_FLOAT_BLOCK: return "ASTC_5x5_FLOAT_BLOCK";
    case Format::ASTC_6x5_FLOAT_BLOCK: return "ASTC_6x5_FLOAT_BLOCK";
    case Format::ASTC_6x6_FLOAT_BLOCK: return "ASTC_6x6_FLOAT_BLOCK";
    case Format::ASTC_8x5_FLOAT_BLOCK: return "ASTC_8x5_FLOAT_BLOCK";
    case Format::ASTC_8x6_FLOAT_BLOCK: return "ASTC_8x6_FLOAT_BLOCK";
    case Format::ASTC_8x8_FLOAT_BLOCK: return "ASTC_8x8_FLOAT_BLOCK";
    case Format::ASTC_10x5_FLOAT_BLOCK: return "ASTC_10x5_FLOAT_BLOCK";
    case Format::ASTC_10x6_FLOAT_BLOCK: return "ASTC_10x6_FLOAT_BLOCK";
    case Format::ASTC_10x8_FLOAT_BLOCK: return "ASTC_10x8_FLOAT_BLOCK";
    case Format::ASTC_10x10_FLOAT_BLOCK: return "ASTC_10x10_FLOAT_BLOCK";
    case Format::ASTC_12x10_FLOAT_BLOCK: return "ASTC_12x10_FLOAT_BLOCK";
    case Format::ASTC_12x12_FLOAT_BLOCK: return "ASTC_12x12_FLOAT_BLOCK";
    case Format::A1BGR5_UNORM_PACK16: return "A1BGR5_UNORM_PACK16";
    case Format::A8_UNORM: return "A8_UNORM";
    case Format::PVRTC1_2BPP_UNORM_BLOCK_IMG: return "PVRTC1_2BPP_UNORM_BLOCK_IMG";
    case Format::PVRTC1_4BPP_UNORM_BLOCK_IMG: return "PVRTC1_4BPP_UNORM_BLOCK_IMG";
    case Format::PVRTC2_2BPP_UNORM_BLOCK_IMG: return "PVRTC2_2BPP_UNORM_BLOCK_IMG";
    case Format::PVRTC2_4BPP_UNORM_BLOCK_IMG: return "PVRTC2_4BPP_UNORM_BLOCK_IMG";
    case Format::PVRTC1_2BPP_SRGB_BLOCK_IMG: return "PVRTC1_2BPP_SRGB_BLOCK_IMG";
    case Format::PVRTC1_4BPP_SRGB_BLOCK_IMG: return "PVRTC1_4BPP_SRGB_BLOCK_IMG";
    case Format::PVRTC2_2BPP_SRGB_BLOCK_IMG: return "PVRTC2_2BPP_SRGB_BLOCK_IMG";
    case Format::PVRTC2_4BPP_SRGB_BLOCK_IMG: return "PVRTC2_4BPP_SRGB_BLOCK_IMG";
    case Format::R8_BOOL_ARM: return "R8_BOOL_ARM";
    case Format::RG16_SFIXED5_NV: return "RG16_SFIXED5_NV";
    case Format::R10X6_UINT_PACK16_ARM: return "R10X6_UINT_PACK16_ARM";
    case Format::R10X6G10X6_UINT_2PACK16_ARM: return "R10X6G10X6_UINT_2PACK16_ARM";
    case Format::R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM: return "R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM";
    case Format::R12X4_UINT_PACK16_ARM: return "R12X4_UINT_PACK16_ARM";
    case Format::R12X4G12X4_UINT_2PACK16_ARM: return "R12X4G12X4_UINT_2PACK16_ARM";
    case Format::R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM: return "R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM";
    case Format::R14X2_UINT_PACK16_ARM: return "R14X2_UINT_PACK16_ARM";
    case Format::R14X2G14X2_UINT_2PACK16_ARM: return "R14X2G14X2_UINT_2PACK16_ARM";
    case Format::R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM: return "R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM";
    case Format::R14X2_UNORM_PACK16_ARM: return "R14X2_UNORM_PACK16_ARM";
    case Format::R14X2G14X2_UNORM_2PACK16_ARM: return "R14X2G14X2_UNORM_2PACK16_ARM";
    case Format::R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM: return "R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM";
    case Format::G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM: return "G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM";
    case Format::G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM: return "G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM";
    default: return "";
    }
}

Format formatFromString(std::string_view format)
{
    using enum Format;
    static const StringUnorderedMap<Format> STRING_TO_FORMAT = {
        std::make_pair(formatToString(Undefined), Undefined),
        std::make_pair(formatToString(RG4_UNORM_PACK8), RG4_UNORM_PACK8),
        std::make_pair(formatToString(RGBA4_UNORM_PACK16), RGBA4_UNORM_PACK16),
        std::make_pair(formatToString(BGRA4_UNORM_PACK16), BGRA4_UNORM_PACK16),
        std::make_pair(formatToString(R5G6B5_UNORM_PACK16), R5G6B5_UNORM_PACK16),
        std::make_pair(formatToString(B5G6R5_UNORM_PACK16), B5G6R5_UNORM_PACK16),
        std::make_pair(formatToString(RGB5A1_UNORM_PACK16), RGB5A1_UNORM_PACK16),
        std::make_pair(formatToString(BGR5A1_UNORM_PACK16), BGR5A1_UNORM_PACK16),
        std::make_pair(formatToString(A1RGB5_UNORM_PACK16), A1RGB5_UNORM_PACK16),
        std::make_pair(formatToString(R8_UNORM), R8_UNORM),
        std::make_pair(formatToString(R8_SNORM), R8_SNORM),
        std::make_pair(formatToString(R8_USCALED), R8_USCALED),
        std::make_pair(formatToString(R8_SSCALED), R8_SSCALED),
        std::make_pair(formatToString(R8_UINT), R8_UINT),
        std::make_pair(formatToString(R8_SINT), R8_SINT),
        std::make_pair(formatToString(R8_SRGB), R8_SRGB),
        std::make_pair(formatToString(RG8_UNORM), RG8_UNORM),
        std::make_pair(formatToString(RG8_SNORM), RG8_SNORM),
        std::make_pair(formatToString(RG8_USCALED), RG8_USCALED),
        std::make_pair(formatToString(RG8_SSCALED), RG8_SSCALED),
        std::make_pair(formatToString(RG8_UINT), RG8_UINT),
        std::make_pair(formatToString(RG8_SINT), RG8_SINT),
        std::make_pair(formatToString(RG8_SRGB), RG8_SRGB),
        std::make_pair(formatToString(RGB8_UNORM), RGB8_UNORM),
        std::make_pair(formatToString(RGB8_SNORM), RGB8_SNORM),
        std::make_pair(formatToString(RGB8_USCALED), RGB8_USCALED),
        std::make_pair(formatToString(RGB8_SSCALED), RGB8_SSCALED),
        std::make_pair(formatToString(RGB8_UINT), RGB8_UINT),
        std::make_pair(formatToString(RGB8_SINT), RGB8_SINT),
        std::make_pair(formatToString(RGB8_SRGB), RGB8_SRGB),
        std::make_pair(formatToString(BGR8_UNORM), BGR8_UNORM),
        std::make_pair(formatToString(BGR8_SNORM), BGR8_SNORM),
        std::make_pair(formatToString(BGR8_USCALED), BGR8_USCALED),
        std::make_pair(formatToString(BGR8_SSCALED), BGR8_SSCALED),
        std::make_pair(formatToString(BGR8_UINT), BGR8_UINT),
        std::make_pair(formatToString(BGR8_SINT), BGR8_SINT),
        std::make_pair(formatToString(BGR8_SRGB), BGR8_SRGB),
        std::make_pair(formatToString(RGBA8_UNORM), RGBA8_UNORM),
        std::make_pair(formatToString(RGBA8_SNORM), RGBA8_SNORM),
        std::make_pair(formatToString(RGBA8_USCALED), RGBA8_USCALED),
        std::make_pair(formatToString(RGBA8_SSCALED), RGBA8_SSCALED),
        std::make_pair(formatToString(RGBA8_UINT), RGBA8_UINT),
        std::make_pair(formatToString(RGBA8_SINT), RGBA8_SINT),
        std::make_pair(formatToString(RGBA8_SRGB), RGBA8_SRGB),
        std::make_pair(formatToString(BGRA8_UNORM), BGRA8_UNORM),
        std::make_pair(formatToString(BGRA8_SNORM), BGRA8_SNORM),
        std::make_pair(formatToString(BGRA8_USCALED), BGRA8_USCALED),
        std::make_pair(formatToString(BGRA8_SSCALED), BGRA8_SSCALED),
        std::make_pair(formatToString(BGRA8_UINT), BGRA8_UINT),
        std::make_pair(formatToString(BGRA8_SINT), BGRA8_SINT),
        std::make_pair(formatToString(BGRA8_SRGB), BGRA8_SRGB),
        std::make_pair(formatToString(ABGR8_UNORM_PACK32), ABGR8_UNORM_PACK32),
        std::make_pair(formatToString(ABGR8_SNORM_PACK32), ABGR8_SNORM_PACK32),
        std::make_pair(formatToString(ABGR8_USCALED_PACK32), ABGR8_USCALED_PACK32),
        std::make_pair(formatToString(ABGR8_SSCALED_PACK32), ABGR8_SSCALED_PACK32),
        std::make_pair(formatToString(ABGR8_UINT_PACK32), ABGR8_UINT_PACK32),
        std::make_pair(formatToString(ABGR8_SINT_PACK32), ABGR8_SINT_PACK32),
        std::make_pair(formatToString(ABGR8_SRGB_PACK32), ABGR8_SRGB_PACK32),
        std::make_pair(formatToString(A2RGB10_UNORM_PACK32), A2RGB10_UNORM_PACK32),
        std::make_pair(formatToString(A2RGB10_SNORM_PACK32), A2RGB10_SNORM_PACK32),
        std::make_pair(formatToString(A2RGB10_USCALED_PACK32), A2RGB10_USCALED_PACK32),
        std::make_pair(formatToString(A2RGB10_SSCALED_PACK32), A2RGB10_SSCALED_PACK32),
        std::make_pair(formatToString(A2RGB10_UINT_PACK32), A2RGB10_UINT_PACK32),
        std::make_pair(formatToString(A2RGB10_SINT_PACK32), A2RGB10_SINT_PACK32),
        std::make_pair(formatToString(A2BGR10_UNORM_PACK32), A2BGR10_UNORM_PACK32),
        std::make_pair(formatToString(A2BGR10_SNORM_PACK32), A2BGR10_SNORM_PACK32),
        std::make_pair(formatToString(A2BGR10_USCALED_PACK32), A2BGR10_USCALED_PACK32),
        std::make_pair(formatToString(A2BGR10_SSCALED_PACK32), A2BGR10_SSCALED_PACK32),
        std::make_pair(formatToString(A2BGR10_UINT_PACK32), A2BGR10_UINT_PACK32),
        std::make_pair(formatToString(A2BGR10_SINT_PACK32), A2BGR10_SINT_PACK32),
        std::make_pair(formatToString(R16_UNORM), R16_UNORM),
        std::make_pair(formatToString(R16_SNORM), R16_SNORM),
        std::make_pair(formatToString(R16_USCALED), R16_USCALED),
        std::make_pair(formatToString(R16_SSCALED), R16_SSCALED),
        std::make_pair(formatToString(R16_UINT), R16_UINT),
        std::make_pair(formatToString(R16_SINT), R16_SINT),
        std::make_pair(formatToString(R16_FLOAT), R16_FLOAT),
        std::make_pair(formatToString(RG16_UNORM), RG16_UNORM),
        std::make_pair(formatToString(RG16_SNORM), RG16_SNORM),
        std::make_pair(formatToString(RG16_USCALED), RG16_USCALED),
        std::make_pair(formatToString(RG16_SSCALED), RG16_SSCALED),
        std::make_pair(formatToString(RG16_UINT), RG16_UINT),
        std::make_pair(formatToString(RG16_SINT), RG16_SINT),
        std::make_pair(formatToString(RG16_FLOAT), RG16_FLOAT),
        std::make_pair(formatToString(RGB16_UNORM), RGB16_UNORM),
        std::make_pair(formatToString(RGB16_SNORM), RGB16_SNORM),
        std::make_pair(formatToString(RGB16_USCALED), RGB16_USCALED),
        std::make_pair(formatToString(RGB16_SSCALED), RGB16_SSCALED),
        std::make_pair(formatToString(RGB16_UINT), RGB16_UINT),
        std::make_pair(formatToString(RGB16_SINT), RGB16_SINT),
        std::make_pair(formatToString(RGB16_FLOAT), RGB16_FLOAT),
        std::make_pair(formatToString(RGBA16_UNORM), RGBA16_UNORM),
        std::make_pair(formatToString(RGBA16_SNORM), RGBA16_SNORM),
        std::make_pair(formatToString(RGBA16_USCALED), RGBA16_USCALED),
        std::make_pair(formatToString(RGBA16_SSCALED), RGBA16_SSCALED),
        std::make_pair(formatToString(RGBA16_UINT), RGBA16_UINT),
        std::make_pair(formatToString(RGBA16_SINT), RGBA16_SINT),
        std::make_pair(formatToString(RGBA16_FLOAT), RGBA16_FLOAT),
        std::make_pair(formatToString(R32_UINT), R32_UINT),
        std::make_pair(formatToString(R32_SINT), R32_SINT),
        std::make_pair(formatToString(R32_FLOAT), R32_FLOAT),
        std::make_pair(formatToString(RG32_UINT), RG32_UINT),
        std::make_pair(formatToString(RG32_SINT), RG32_SINT),
        std::make_pair(formatToString(RG32_FLOAT), RG32_FLOAT),
        std::make_pair(formatToString(RGB32_UINT), RGB32_UINT),
        std::make_pair(formatToString(RGB32_SINT), RGB32_SINT),
        std::make_pair(formatToString(RGB32_FLOAT), RGB32_FLOAT),
        std::make_pair(formatToString(RGBA32_UINT), RGBA32_UINT),
        std::make_pair(formatToString(RGBA32_SINT), RGBA32_SINT),
        std::make_pair(formatToString(RGBA32_FLOAT), RGBA32_FLOAT),
        std::make_pair(formatToString(R64_UINT), R64_UINT),
        std::make_pair(formatToString(R64_SINT), R64_SINT),
        std::make_pair(formatToString(R64_FLOAT), R64_FLOAT),
        std::make_pair(formatToString(RG64_UINT), RG64_UINT),
        std::make_pair(formatToString(RG64_SINT), RG64_SINT),
        std::make_pair(formatToString(RG64_FLOAT), RG64_FLOAT),
        std::make_pair(formatToString(RGB64_UINT), RGB64_UINT),
        std::make_pair(formatToString(RGB64_SINT), RGB64_SINT),
        std::make_pair(formatToString(RGB64_FLOAT), RGB64_FLOAT),
        std::make_pair(formatToString(RGBA64_UINT), RGBA64_UINT),
        std::make_pair(formatToString(RGBA64_SINT), RGBA64_SINT),
        std::make_pair(formatToString(RGBA64_FLOAT), RGBA64_FLOAT),
        std::make_pair(formatToString(B10G11R11_UFLOAT_PACK32), B10G11R11_UFLOAT_PACK32),
        std::make_pair(formatToString(E5BGR9_UFLOAT_PACK32), E5BGR9_UFLOAT_PACK32),
        std::make_pair(formatToString(D16_UNORM), D16_UNORM),
        std::make_pair(formatToString(X8_D24_UNORM_PACK32), X8_D24_UNORM_PACK32),
        std::make_pair(formatToString(D32_FLOAT), D32_FLOAT),
        std::make_pair(formatToString(S8_UINT), S8_UINT),
        std::make_pair(formatToString(D16_UNORM_S8_UINT), D16_UNORM_S8_UINT),
        std::make_pair(formatToString(D24_UNORM_S8_UINT), D24_UNORM_S8_UINT),
        std::make_pair(formatToString(D32_FLOAT_S8_UINT), D32_FLOAT_S8_UINT),
        std::make_pair(formatToString(BC1_RGB_UNORM_BLOCK), BC1_RGB_UNORM_BLOCK),
        std::make_pair(formatToString(BC1_RGB_SRGB_BLOCK), BC1_RGB_SRGB_BLOCK),
        std::make_pair(formatToString(BC1_RGBA_UNORM_BLOCK), BC1_RGBA_UNORM_BLOCK),
        std::make_pair(formatToString(BC1_RGBA_SRGB_BLOCK), BC1_RGBA_SRGB_BLOCK),
        std::make_pair(formatToString(BC2_UNORM_BLOCK), BC2_UNORM_BLOCK),
        std::make_pair(formatToString(BC2_SRGB_BLOCK), BC2_SRGB_BLOCK),
        std::make_pair(formatToString(BC3_UNORM_BLOCK), BC3_UNORM_BLOCK),
        std::make_pair(formatToString(BC3_SRGB_BLOCK), BC3_SRGB_BLOCK),
        std::make_pair(formatToString(BC4_UNORM_BLOCK), BC4_UNORM_BLOCK),
        std::make_pair(formatToString(BC4_SNORM_BLOCK), BC4_SNORM_BLOCK),
        std::make_pair(formatToString(BC5_UNORM_BLOCK), BC5_UNORM_BLOCK),
        std::make_pair(formatToString(BC5_SNORM_BLOCK), BC5_SNORM_BLOCK),
        std::make_pair(formatToString(BC6H_UFLOAT_BLOCK), BC6H_UFLOAT_BLOCK),
        std::make_pair(formatToString(BC6H_FLOAT_BLOCK), BC6H_FLOAT_BLOCK),
        std::make_pair(formatToString(BC7_UNORM_BLOCK), BC7_UNORM_BLOCK),
        std::make_pair(formatToString(BC7_SRGB_BLOCK), BC7_SRGB_BLOCK),
        std::make_pair(formatToString(ETC2_RGB8_UNORM_BLOCK), ETC2_RGB8_UNORM_BLOCK),
        std::make_pair(formatToString(ETC2_RGB8_SRGB_BLOCK), ETC2_RGB8_SRGB_BLOCK),
        std::make_pair(formatToString(ETC2_RGB8A1_UNORM_BLOCK), ETC2_RGB8A1_UNORM_BLOCK),
        std::make_pair(formatToString(ETC2_RGB8A1_SRGB_BLOCK), ETC2_RGB8A1_SRGB_BLOCK),
        std::make_pair(formatToString(ETC2_RGBA8_UNORM_BLOCK), ETC2_RGBA8_UNORM_BLOCK),
        std::make_pair(formatToString(ETC2_RGBA8_SRGB_BLOCK), ETC2_RGBA8_SRGB_BLOCK),
        std::make_pair(formatToString(EAC_R11_UNORM_BLOCK), EAC_R11_UNORM_BLOCK),
        std::make_pair(formatToString(EAC_R11_SNORM_BLOCK), EAC_R11_SNORM_BLOCK),
        std::make_pair(formatToString(EAC_R11G11_UNORM_BLOCK), EAC_R11G11_UNORM_BLOCK),
        std::make_pair(formatToString(EAC_R11G11_SNORM_BLOCK), EAC_R11G11_SNORM_BLOCK),
        std::make_pair(formatToString(ASTC_4x4_UNORM_BLOCK), ASTC_4x4_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_4x4_SRGB_BLOCK), ASTC_4x4_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_5x4_UNORM_BLOCK), ASTC_5x4_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_5x4_SRGB_BLOCK), ASTC_5x4_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_5x5_UNORM_BLOCK), ASTC_5x5_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_5x5_SRGB_BLOCK), ASTC_5x5_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_6x5_UNORM_BLOCK), ASTC_6x5_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_6x5_SRGB_BLOCK), ASTC_6x5_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_6x6_UNORM_BLOCK), ASTC_6x6_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_6x6_SRGB_BLOCK), ASTC_6x6_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_8x5_UNORM_BLOCK), ASTC_8x5_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_8x5_SRGB_BLOCK), ASTC_8x5_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_8x6_UNORM_BLOCK), ASTC_8x6_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_8x6_SRGB_BLOCK), ASTC_8x6_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_8x8_UNORM_BLOCK), ASTC_8x8_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_8x8_SRGB_BLOCK), ASTC_8x8_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_10x5_UNORM_BLOCK), ASTC_10x5_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_10x5_SRGB_BLOCK), ASTC_10x5_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_10x6_UNORM_BLOCK), ASTC_10x6_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_10x6_SRGB_BLOCK), ASTC_10x6_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_10x8_UNORM_BLOCK), ASTC_10x8_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_10x8_SRGB_BLOCK), ASTC_10x8_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_10x10_UNORM_BLOCK), ASTC_10x10_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_10x10_SRGB_BLOCK), ASTC_10x10_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_12x10_UNORM_BLOCK), ASTC_12x10_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_12x10_SRGB_BLOCK), ASTC_12x10_SRGB_BLOCK),
        std::make_pair(formatToString(ASTC_12x12_UNORM_BLOCK), ASTC_12x12_UNORM_BLOCK),
        std::make_pair(formatToString(ASTC_12x12_SRGB_BLOCK), ASTC_12x12_SRGB_BLOCK),
        std::make_pair(formatToString(GBGR8_422_UNORM), GBGR8_422_UNORM),
        std::make_pair(formatToString(B8G8RG8_422_UNORM), B8G8RG8_422_UNORM),
        std::make_pair(formatToString(G8_B8_R8_3PLANE_420_UNORM), G8_B8_R8_3PLANE_420_UNORM),
        std::make_pair(formatToString(G8_B8R8_2PLANE_420_UNORM), G8_B8R8_2PLANE_420_UNORM),
        std::make_pair(formatToString(G8_B8_R8_3PLANE_422_UNORM), G8_B8_R8_3PLANE_422_UNORM),
        std::make_pair(formatToString(G8_B8R8_2PLANE_422_UNORM), G8_B8R8_2PLANE_422_UNORM),
        std::make_pair(formatToString(G8_B8_R8_3PLANE_444_UNORM), G8_B8_R8_3PLANE_444_UNORM),
        std::make_pair(formatToString(R10X6_UNORM_PACK16), R10X6_UNORM_PACK16),
        std::make_pair(formatToString(R10X6G10X6_UNORM_2PACK16), R10X6G10X6_UNORM_2PACK16),
        std::make_pair(formatToString(R10X6G10X6B10X6A10X6_UNORM_4PACK16), R10X6G10X6B10X6A10X6_UNORM_4PACK16),
        std::make_pair(formatToString(G10X6B10X6G10X6R10X6_422_UNORM_4PACK16), G10X6B10X6G10X6R10X6_422_UNORM_4PACK16),
        std::make_pair(formatToString(B10X6G10X6R10X6G10X6_422_UNORM_4PACK16), B10X6G10X6R10X6G10X6_422_UNORM_4PACK16),
        std::make_pair(formatToString(G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16),
            G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16),
        std::make_pair(formatToString(G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16),
            G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16),
        std::make_pair(formatToString(G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16),
            G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16),
        std::make_pair(formatToString(G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16),
            G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16),
        std::make_pair(formatToString(G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16),
            G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16),
        std::make_pair(formatToString(R12X4_UNORM_PACK16), R12X4_UNORM_PACK16),
        std::make_pair(formatToString(R12X4G12X4_UNORM_2PACK16), R12X4G12X4_UNORM_2PACK16),
        std::make_pair(formatToString(R12X4G12X4B12X4A12X4_UNORM_4PACK16), R12X4G12X4B12X4A12X4_UNORM_4PACK16),
        std::make_pair(formatToString(G12X4B12X4G12X4R12X4_422_UNORM_4PACK16), G12X4B12X4G12X4R12X4_422_UNORM_4PACK16),
        std::make_pair(formatToString(B12X4G12X4R12X4G12X4_422_UNORM_4PACK16), B12X4G12X4R12X4G12X4_422_UNORM_4PACK16),
        std::make_pair(formatToString(G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16),
            G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16),
        std::make_pair(formatToString(G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16),
            G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16),
        std::make_pair(formatToString(G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16),
            G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16),
        std::make_pair(formatToString(G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16),
            G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16),
        std::make_pair(formatToString(G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16),
            G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16),
        std::make_pair(formatToString(G16B16G16R16_422_UNORM), G16B16G16R16_422_UNORM),
        std::make_pair(formatToString(B16G16RG16_422_UNORM), B16G16RG16_422_UNORM),
        std::make_pair(formatToString(G16_B16_R16_3PLANE_420_UNORM), G16_B16_R16_3PLANE_420_UNORM),
        std::make_pair(formatToString(G16_B16R16_2PLANE_420_UNORM), G16_B16R16_2PLANE_420_UNORM),
        std::make_pair(formatToString(G16_B16_R16_3PLANE_422_UNORM), G16_B16_R16_3PLANE_422_UNORM),
        std::make_pair(formatToString(G16_B16R16_2PLANE_422_UNORM), G16_B16R16_2PLANE_422_UNORM),
        std::make_pair(formatToString(G16_B16_R16_3PLANE_444_UNORM), G16_B16_R16_3PLANE_444_UNORM),
        std::make_pair(formatToString(G8_B8R8_2PLANE_444_UNORM), G8_B8R8_2PLANE_444_UNORM),
        std::make_pair(formatToString(G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16),
            G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16),
        std::make_pair(formatToString(G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16),
            G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16),
        std::make_pair(formatToString(G16_B16R16_2PLANE_444_UNORM), G16_B16R16_2PLANE_444_UNORM),
        std::make_pair(formatToString(A4RGB4_UNORM_PACK16), A4RGB4_UNORM_PACK16),
        std::make_pair(formatToString(A4B4G4R4_UNORM_PACK16), A4B4G4R4_UNORM_PACK16),
        std::make_pair(formatToString(ASTC_4x4_FLOAT_BLOCK), ASTC_4x4_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_5x4_FLOAT_BLOCK), ASTC_5x4_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_5x5_FLOAT_BLOCK), ASTC_5x5_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_6x5_FLOAT_BLOCK), ASTC_6x5_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_6x6_FLOAT_BLOCK), ASTC_6x6_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_8x5_FLOAT_BLOCK), ASTC_8x5_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_8x6_FLOAT_BLOCK), ASTC_8x6_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_8x8_FLOAT_BLOCK), ASTC_8x8_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_10x5_FLOAT_BLOCK), ASTC_10x5_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_10x6_FLOAT_BLOCK), ASTC_10x6_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_10x8_FLOAT_BLOCK), ASTC_10x8_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_10x10_FLOAT_BLOCK), ASTC_10x10_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_12x10_FLOAT_BLOCK), ASTC_12x10_FLOAT_BLOCK),
        std::make_pair(formatToString(ASTC_12x12_FLOAT_BLOCK), ASTC_12x12_FLOAT_BLOCK),
        std::make_pair(formatToString(A1BGR5_UNORM_PACK16), A1BGR5_UNORM_PACK16),
        std::make_pair(formatToString(A8_UNORM), A8_UNORM),
        std::make_pair(formatToString(PVRTC1_2BPP_UNORM_BLOCK_IMG), PVRTC1_2BPP_UNORM_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC1_4BPP_UNORM_BLOCK_IMG), PVRTC1_4BPP_UNORM_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC2_2BPP_UNORM_BLOCK_IMG), PVRTC2_2BPP_UNORM_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC2_4BPP_UNORM_BLOCK_IMG), PVRTC2_4BPP_UNORM_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC1_2BPP_SRGB_BLOCK_IMG), PVRTC1_2BPP_SRGB_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC1_4BPP_SRGB_BLOCK_IMG), PVRTC1_4BPP_SRGB_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC2_2BPP_SRGB_BLOCK_IMG), PVRTC2_2BPP_SRGB_BLOCK_IMG),
        std::make_pair(formatToString(PVRTC2_4BPP_SRGB_BLOCK_IMG), PVRTC2_4BPP_SRGB_BLOCK_IMG),
        std::make_pair(formatToString(R8_BOOL_ARM), R8_BOOL_ARM),
        std::make_pair(formatToString(RG16_SFIXED5_NV), RG16_SFIXED5_NV),
        std::make_pair(formatToString(R10X6_UINT_PACK16_ARM), R10X6_UINT_PACK16_ARM),
        std::make_pair(formatToString(R10X6G10X6_UINT_2PACK16_ARM), R10X6G10X6_UINT_2PACK16_ARM),
        std::make_pair(formatToString(R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM), R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM),
        std::make_pair(formatToString(R12X4_UINT_PACK16_ARM), R12X4_UINT_PACK16_ARM),
        std::make_pair(formatToString(R12X4G12X4_UINT_2PACK16_ARM), R12X4G12X4_UINT_2PACK16_ARM),
        std::make_pair(formatToString(R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM), R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM),
        std::make_pair(formatToString(R14X2_UINT_PACK16_ARM), R14X2_UINT_PACK16_ARM),
        std::make_pair(formatToString(R14X2G14X2_UINT_2PACK16_ARM), R14X2G14X2_UINT_2PACK16_ARM),
        std::make_pair(formatToString(R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM), R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM),
        std::make_pair(formatToString(R14X2_UNORM_PACK16_ARM), R14X2_UNORM_PACK16_ARM),
        std::make_pair(formatToString(R14X2G14X2_UNORM_2PACK16_ARM), R14X2G14X2_UNORM_2PACK16_ARM),
        std::make_pair(formatToString(R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM), R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM),
        std::make_pair(formatToString(G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM),
            G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM),
        std::make_pair(formatToString(G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM),
            G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM)
    };

    return STRING_TO_FORMAT.find(format)->second;
}
}
