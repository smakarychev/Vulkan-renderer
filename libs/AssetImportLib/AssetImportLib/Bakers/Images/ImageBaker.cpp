#include "ImageBaker.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Bakers/BakersUtils.h>
#include <CoreLib/Utils/FileUtils.h>

#include <glaze/glaze.hpp>
#include <ktx.h>
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <ktx/command_create.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

#define CHECK_RETURN_IO_ERROR_PROPAGATE(result) \
ASSETLIB_CHECK_RETURN_IO_ERROR_PROPAGATE(result)

namespace lux::import
{
std::filesystem::path ImageBaker::GetBakedPath(const std::filesystem::path& metaPath) const
{
    auto metaRead = assetlib::image::readMeta(metaPath);
    ASSERT(metaRead.has_value())
    if (!metaRead.has_value())
        return {};
    
    return GetBakedPath(*metaRead);
}

std::filesystem::path ImageBaker::GetBakedPath(const assetlib::ImageMeta& meta) const
{
    return getPostBakePath(meta.Metadata, POST_BAKE_EXTENSION, *m_Ctx);
}

IoResult<std::filesystem::path> ImageBaker::BakeToFile(assetlib::ImageMeta& meta, 
    const std::filesystem::path& metaPath)
{
    ASSERT(!meta.Metadata.Io.OriginalFile.empty())

    const AssetPaths paths = getPostBakePaths(meta.Metadata, POST_BAKE_EXTENSION, *m_Ctx);
    auto baked = Bake(meta);
    CHECK_RETURN_IO_ERROR_PROPAGATE(baked)

    u64 binarySizeBytes = 0;
    for (auto& mip : baked->Header.MipmapSizes)
        for (u32 layer = 0; layer < baked->Header.Layers; layer++)
            binarySizeBytes += mip[layer];
    
    auto packedImage = assetlib::image::pack(*baked, *m_Ctx->Compressor);
    CHECK_RETURN_IO_ERROR_PROPAGATE(packedImage)

    meta.Metadata.Io = {
        .OriginalFile = meta.Metadata.Io.OriginalFile,
        .HeaderFile = std::filesystem::weakly_canonical(paths.HeaderPath).generic_string(),
        .BinaryFile = std::filesystem::weakly_canonical(paths.BinaryPath).generic_string(),
        .BinarySizeBytes = binarySizeBytes,
        .BinarySizeBytesCompressed = packedImage->PackedBinaries.size(),
        .BinarySizeBytesChunksCompressed = std::move(packedImage->PackedBinarySizeBytesChunks),
        .IoMode = m_Ctx->Io->GetName(),
        .CompressionMode = m_Ctx->Compressor->GetName(),
        .IoGuid = m_Ctx->Io->GetGuid(),
        .CompressionGuid = m_Ctx->Compressor->GetGuid()
    };
    {
        auto updatedMeta = assetlib::image::packMeta(meta);
        CHECK_RETURN_IO_ERROR_PROPAGATE(updatedMeta)
        auto writeResult = writeStringToFile(metaPath, assetlib::io::getAssetHeaderFormatted(*updatedMeta));
        CHECK_RETURN_IO_ERROR(writeResult.has_value(), IoError::ErrorCode::GeneralError,
            "Failed to update meta file for {}", metaPath.string())
    }

    IoResult<void> saveResult = m_Ctx->Io->WriteHeader(meta.Metadata, packedImage->Header);
    CHECK_RETURN_IO_ERROR_PROPAGATE(saveResult)

    IoResult<u64> binarySaveResult = m_Ctx->Io->WriteBinaryChunk(meta.Metadata, packedImage->PackedBinaries);
    CHECK_RETURN_IO_ERROR_PROPAGATE(binarySaveResult)

    return paths.HeaderPath;
}

bool ImageBaker::ShouldBake(const std::filesystem::path& metaPath) const
{
    namespace fs = std::filesystem;
    
    ASSERT(metaPath.extension().string() == assetlib::ASSETLIB_METADATA_EXTENSION)
    
    auto metaRead = assetlib::image::readMeta(metaPath);
    if (!metaRead.has_value())
        return true;
    
    const std::filesystem::path rawPath = metaRead->Metadata.Io.OriginalFile;
    const std::filesystem::path bakedPath = GetBakedPath(*metaRead);
    
    if (!fs::exists(bakedPath))
        return true;

    const auto lastBaked = fs::last_write_time(bakedPath);
    if (lastBaked < fs::last_write_time(metaPath) || lastBaked < fs::last_write_time(rawPath))
        return true;

    IoResult<assetlib::AssetCustomHeaderType> assetFileRead = m_Ctx->Io->ReadHeader(metaRead->Metadata);
    if (!assetFileRead.has_value())
        return true;

    const auto unpackHeader = assetlib::image::readHeader(metaRead->Metadata);
    if (!unpackHeader.has_value())
        return true;

    return false; 
}

IoResult<assetlib::ImageAsset> ImageBaker::Bake(const assetlib::ImageMeta& meta)
{
    const std::filesystem::path imagePath = meta.Metadata.Io.OriginalFile;
    if (imagePath.extension() == IMAGE_ASSET_RAW_HDR_EXTENSION)
        return BakeHDR(meta);
    if (imagePath.extension() == IMAGE_ASSET_RAW_JPG_EXTENSION ||
        imagePath.extension() == IMAGE_ASSET_RAW_JPEG_EXTENSION)
        return BakeLDRJpg(meta);
    return BakeLDRKtx(meta);
}

namespace 
{
VkFormat vkFormatFromImageFormat(assetlib::ImageFormat format)
{
    static_assert((u32)assetlib::ImageFormat::Undefined == VK_FORMAT_UNDEFINED);
    static_assert((u32)assetlib::ImageFormat::RG4_UNORM_PACK8 == VK_FORMAT_R4G4_UNORM_PACK8);
    static_assert((u32)assetlib::ImageFormat::RGBA4_UNORM_PACK16 == VK_FORMAT_R4G4B4A4_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::BGRA4_UNORM_PACK16 == VK_FORMAT_B4G4R4A4_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::R5G6B5_UNORM_PACK16 == VK_FORMAT_R5G6B5_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::B5G6R5_UNORM_PACK16 == VK_FORMAT_B5G6R5_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::RGB5A1_UNORM_PACK16 == VK_FORMAT_R5G5B5A1_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::BGR5A1_UNORM_PACK16 == VK_FORMAT_B5G5R5A1_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::A1RGB5_UNORM_PACK16 == VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::R8_UNORM == VK_FORMAT_R8_UNORM);
    static_assert((u32)assetlib::ImageFormat::R8_SNORM == VK_FORMAT_R8_SNORM);
    static_assert((u32)assetlib::ImageFormat::R8_USCALED == VK_FORMAT_R8_USCALED);
    static_assert((u32)assetlib::ImageFormat::R8_SSCALED == VK_FORMAT_R8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::R8_UINT == VK_FORMAT_R8_UINT);
    static_assert((u32)assetlib::ImageFormat::R8_SINT == VK_FORMAT_R8_SINT);
    static_assert((u32)assetlib::ImageFormat::R8_SRGB == VK_FORMAT_R8_SRGB);
    static_assert((u32)assetlib::ImageFormat::RG8_UNORM == VK_FORMAT_R8G8_UNORM);
    static_assert((u32)assetlib::ImageFormat::RG8_SNORM == VK_FORMAT_R8G8_SNORM);
    static_assert((u32)assetlib::ImageFormat::RG8_USCALED == VK_FORMAT_R8G8_USCALED);
    static_assert((u32)assetlib::ImageFormat::RG8_SSCALED == VK_FORMAT_R8G8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RG8_UINT == VK_FORMAT_R8G8_UINT);
    static_assert((u32)assetlib::ImageFormat::RG8_SINT == VK_FORMAT_R8G8_SINT);
    static_assert((u32)assetlib::ImageFormat::RG8_SRGB == VK_FORMAT_R8G8_SRGB);
    static_assert((u32)assetlib::ImageFormat::RGB8_UNORM == VK_FORMAT_R8G8B8_UNORM);
    static_assert((u32)assetlib::ImageFormat::RGB8_SNORM == VK_FORMAT_R8G8B8_SNORM);
    static_assert((u32)assetlib::ImageFormat::RGB8_USCALED == VK_FORMAT_R8G8B8_USCALED);
    static_assert((u32)assetlib::ImageFormat::RGB8_SSCALED == VK_FORMAT_R8G8B8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RGB8_UINT == VK_FORMAT_R8G8B8_UINT);
    static_assert((u32)assetlib::ImageFormat::RGB8_SINT == VK_FORMAT_R8G8B8_SINT);
    static_assert((u32)assetlib::ImageFormat::RGB8_SRGB == VK_FORMAT_R8G8B8_SRGB);
    static_assert((u32)assetlib::ImageFormat::BGR8_UNORM == VK_FORMAT_B8G8R8_UNORM);
    static_assert((u32)assetlib::ImageFormat::BGR8_SNORM == VK_FORMAT_B8G8R8_SNORM);
    static_assert((u32)assetlib::ImageFormat::BGR8_USCALED == VK_FORMAT_B8G8R8_USCALED);
    static_assert((u32)assetlib::ImageFormat::BGR8_SSCALED == VK_FORMAT_B8G8R8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::BGR8_UINT == VK_FORMAT_B8G8R8_UINT);
    static_assert((u32)assetlib::ImageFormat::BGR8_SINT == VK_FORMAT_B8G8R8_SINT);
    static_assert((u32)assetlib::ImageFormat::BGR8_SRGB == VK_FORMAT_B8G8R8_SRGB);
    static_assert((u32)assetlib::ImageFormat::RGBA8_UNORM == VK_FORMAT_R8G8B8A8_UNORM);
    static_assert((u32)assetlib::ImageFormat::RGBA8_SNORM == VK_FORMAT_R8G8B8A8_SNORM);
    static_assert((u32)assetlib::ImageFormat::RGBA8_USCALED == VK_FORMAT_R8G8B8A8_USCALED);
    static_assert((u32)assetlib::ImageFormat::RGBA8_SSCALED == VK_FORMAT_R8G8B8A8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RGBA8_UINT == VK_FORMAT_R8G8B8A8_UINT);
    static_assert((u32)assetlib::ImageFormat::RGBA8_SINT == VK_FORMAT_R8G8B8A8_SINT);
    static_assert((u32)assetlib::ImageFormat::RGBA8_SRGB == VK_FORMAT_R8G8B8A8_SRGB);
    static_assert((u32)assetlib::ImageFormat::BGRA8_UNORM == VK_FORMAT_B8G8R8A8_UNORM);
    static_assert((u32)assetlib::ImageFormat::BGRA8_SNORM == VK_FORMAT_B8G8R8A8_SNORM);
    static_assert((u32)assetlib::ImageFormat::BGRA8_USCALED == VK_FORMAT_B8G8R8A8_USCALED);
    static_assert((u32)assetlib::ImageFormat::BGRA8_SSCALED == VK_FORMAT_B8G8R8A8_SSCALED);
    static_assert((u32)assetlib::ImageFormat::BGRA8_UINT == VK_FORMAT_B8G8R8A8_UINT);
    static_assert((u32)assetlib::ImageFormat::BGRA8_SINT == VK_FORMAT_B8G8R8A8_SINT);
    static_assert((u32)assetlib::ImageFormat::BGRA8_SRGB == VK_FORMAT_B8G8R8A8_SRGB);
    static_assert((u32)assetlib::ImageFormat::ABGR8_UNORM_PACK32 == VK_FORMAT_A8B8G8R8_UNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_SNORM_PACK32 == VK_FORMAT_A8B8G8R8_SNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_USCALED_PACK32 == VK_FORMAT_A8B8G8R8_USCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_SSCALED_PACK32 == VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_UINT_PACK32 == VK_FORMAT_A8B8G8R8_UINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_SINT_PACK32 == VK_FORMAT_A8B8G8R8_SINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::ABGR8_SRGB_PACK32 == VK_FORMAT_A8B8G8R8_SRGB_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_UNORM_PACK32 == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_SNORM_PACK32 == VK_FORMAT_A2R10G10B10_SNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_USCALED_PACK32 == VK_FORMAT_A2R10G10B10_USCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_SSCALED_PACK32 == VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_UINT_PACK32 == VK_FORMAT_A2R10G10B10_UINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2RGB10_SINT_PACK32 == VK_FORMAT_A2R10G10B10_SINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_UNORM_PACK32 == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_SNORM_PACK32 == VK_FORMAT_A2B10G10R10_SNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_USCALED_PACK32 == VK_FORMAT_A2B10G10R10_USCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_SSCALED_PACK32 == VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_UINT_PACK32 == VK_FORMAT_A2B10G10R10_UINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::A2BGR10_SINT_PACK32 == VK_FORMAT_A2B10G10R10_SINT_PACK32);
    static_assert((u32)assetlib::ImageFormat::R16_UNORM == VK_FORMAT_R16_UNORM);
    static_assert((u32)assetlib::ImageFormat::R16_SNORM == VK_FORMAT_R16_SNORM);
    static_assert((u32)assetlib::ImageFormat::R16_USCALED == VK_FORMAT_R16_USCALED);
    static_assert((u32)assetlib::ImageFormat::R16_SSCALED == VK_FORMAT_R16_SSCALED);
    static_assert((u32)assetlib::ImageFormat::R16_UINT == VK_FORMAT_R16_UINT);
    static_assert((u32)assetlib::ImageFormat::R16_SINT == VK_FORMAT_R16_SINT);
    static_assert((u32)assetlib::ImageFormat::R16_FLOAT == VK_FORMAT_R16_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RG16_UNORM == VK_FORMAT_R16G16_UNORM);
    static_assert((u32)assetlib::ImageFormat::RG16_SNORM == VK_FORMAT_R16G16_SNORM);
    static_assert((u32)assetlib::ImageFormat::RG16_USCALED == VK_FORMAT_R16G16_USCALED);
    static_assert((u32)assetlib::ImageFormat::RG16_SSCALED == VK_FORMAT_R16G16_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RG16_UINT == VK_FORMAT_R16G16_UINT);
    static_assert((u32)assetlib::ImageFormat::RG16_SINT == VK_FORMAT_R16G16_SINT);
    static_assert((u32)assetlib::ImageFormat::RG16_FLOAT == VK_FORMAT_R16G16_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGB16_UNORM == VK_FORMAT_R16G16B16_UNORM);
    static_assert((u32)assetlib::ImageFormat::RGB16_SNORM == VK_FORMAT_R16G16B16_SNORM);
    static_assert((u32)assetlib::ImageFormat::RGB16_USCALED == VK_FORMAT_R16G16B16_USCALED);
    static_assert((u32)assetlib::ImageFormat::RGB16_SSCALED == VK_FORMAT_R16G16B16_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RGB16_UINT == VK_FORMAT_R16G16B16_UINT);
    static_assert((u32)assetlib::ImageFormat::RGB16_SINT == VK_FORMAT_R16G16B16_SINT);
    static_assert((u32)assetlib::ImageFormat::RGB16_FLOAT == VK_FORMAT_R16G16B16_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGBA16_UNORM == VK_FORMAT_R16G16B16A16_UNORM);
    static_assert((u32)assetlib::ImageFormat::RGBA16_SNORM == VK_FORMAT_R16G16B16A16_SNORM);
    static_assert((u32)assetlib::ImageFormat::RGBA16_USCALED == VK_FORMAT_R16G16B16A16_USCALED);
    static_assert((u32)assetlib::ImageFormat::RGBA16_SSCALED == VK_FORMAT_R16G16B16A16_SSCALED);
    static_assert((u32)assetlib::ImageFormat::RGBA16_UINT == VK_FORMAT_R16G16B16A16_UINT);
    static_assert((u32)assetlib::ImageFormat::RGBA16_SINT == VK_FORMAT_R16G16B16A16_SINT);
    static_assert((u32)assetlib::ImageFormat::RGBA16_FLOAT == VK_FORMAT_R16G16B16A16_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::R32_UINT == VK_FORMAT_R32_UINT);
    static_assert((u32)assetlib::ImageFormat::R32_SINT == VK_FORMAT_R32_SINT);
    static_assert((u32)assetlib::ImageFormat::R32_FLOAT == VK_FORMAT_R32_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RG32_UINT == VK_FORMAT_R32G32_UINT);
    static_assert((u32)assetlib::ImageFormat::RG32_SINT == VK_FORMAT_R32G32_SINT);
    static_assert((u32)assetlib::ImageFormat::RG32_FLOAT == VK_FORMAT_R32G32_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGB32_UINT == VK_FORMAT_R32G32B32_UINT);
    static_assert((u32)assetlib::ImageFormat::RGB32_SINT == VK_FORMAT_R32G32B32_SINT);
    static_assert((u32)assetlib::ImageFormat::RGB32_FLOAT == VK_FORMAT_R32G32B32_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGBA32_UINT == VK_FORMAT_R32G32B32A32_UINT);
    static_assert((u32)assetlib::ImageFormat::RGBA32_SINT == VK_FORMAT_R32G32B32A32_SINT);
    static_assert((u32)assetlib::ImageFormat::RGBA32_FLOAT == VK_FORMAT_R32G32B32A32_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::R64_UINT == VK_FORMAT_R64_UINT);
    static_assert((u32)assetlib::ImageFormat::R64_SINT == VK_FORMAT_R64_SINT);
    static_assert((u32)assetlib::ImageFormat::R64_FLOAT == VK_FORMAT_R64_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RG64_UINT == VK_FORMAT_R64G64_UINT);
    static_assert((u32)assetlib::ImageFormat::RG64_SINT == VK_FORMAT_R64G64_SINT);
    static_assert((u32)assetlib::ImageFormat::RG64_FLOAT == VK_FORMAT_R64G64_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGB64_UINT == VK_FORMAT_R64G64B64_UINT);
    static_assert((u32)assetlib::ImageFormat::RGB64_SINT == VK_FORMAT_R64G64B64_SINT);
    static_assert((u32)assetlib::ImageFormat::RGB64_FLOAT == VK_FORMAT_R64G64B64_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::RGBA64_UINT == VK_FORMAT_R64G64B64A64_UINT);
    static_assert((u32)assetlib::ImageFormat::RGBA64_SINT == VK_FORMAT_R64G64B64A64_SINT);
    static_assert((u32)assetlib::ImageFormat::RGBA64_FLOAT == VK_FORMAT_R64G64B64A64_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::B10G11R11_UFLOAT_PACK32 == VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    static_assert((u32)assetlib::ImageFormat::E5BGR9_UFLOAT_PACK32 == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
    static_assert((u32)assetlib::ImageFormat::D16_UNORM == VK_FORMAT_D16_UNORM);
    static_assert((u32)assetlib::ImageFormat::X8_D24_UNORM_PACK32 == VK_FORMAT_X8_D24_UNORM_PACK32);
    static_assert((u32)assetlib::ImageFormat::D32_FLOAT == VK_FORMAT_D32_SFLOAT);
    static_assert((u32)assetlib::ImageFormat::S8_UINT == VK_FORMAT_S8_UINT);
    static_assert((u32)assetlib::ImageFormat::D16_UNORM_S8_UINT == VK_FORMAT_D16_UNORM_S8_UINT);
    static_assert((u32)assetlib::ImageFormat::D24_UNORM_S8_UINT == VK_FORMAT_D24_UNORM_S8_UINT);
    static_assert((u32)assetlib::ImageFormat::D32_FLOAT_S8_UINT == VK_FORMAT_D32_SFLOAT_S8_UINT);
    static_assert((u32)assetlib::ImageFormat::BC1_RGB_UNORM_BLOCK == VK_FORMAT_BC1_RGB_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC1_RGB_SRGB_BLOCK == VK_FORMAT_BC1_RGB_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC1_RGBA_UNORM_BLOCK == VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC1_RGBA_SRGB_BLOCK == VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC2_UNORM_BLOCK == VK_FORMAT_BC2_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC2_SRGB_BLOCK == VK_FORMAT_BC2_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC3_UNORM_BLOCK == VK_FORMAT_BC3_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC3_SRGB_BLOCK == VK_FORMAT_BC3_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC4_UNORM_BLOCK == VK_FORMAT_BC4_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC4_SNORM_BLOCK == VK_FORMAT_BC4_SNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC5_UNORM_BLOCK == VK_FORMAT_BC5_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC5_SNORM_BLOCK == VK_FORMAT_BC5_SNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC6H_UFLOAT_BLOCK == VK_FORMAT_BC6H_UFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC6H_FLOAT_BLOCK == VK_FORMAT_BC6H_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC7_UNORM_BLOCK == VK_FORMAT_BC7_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::BC7_SRGB_BLOCK == VK_FORMAT_BC7_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGB8_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGB8_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGB8A1_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGB8A1_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGBA8_UNORM_BLOCK == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ETC2_RGBA8_SRGB_BLOCK == VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::EAC_R11_UNORM_BLOCK == VK_FORMAT_EAC_R11_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::EAC_R11_SNORM_BLOCK == VK_FORMAT_EAC_R11_SNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::EAC_R11G11_UNORM_BLOCK == VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::EAC_R11G11_SNORM_BLOCK == VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_4x4_UNORM_BLOCK == VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_4x4_SRGB_BLOCK == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x4_UNORM_BLOCK == VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x4_SRGB_BLOCK == VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x5_UNORM_BLOCK == VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x5_SRGB_BLOCK == VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x5_UNORM_BLOCK == VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x5_SRGB_BLOCK == VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x6_UNORM_BLOCK == VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x6_SRGB_BLOCK == VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x5_UNORM_BLOCK == VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x5_SRGB_BLOCK == VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x6_UNORM_BLOCK == VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x6_SRGB_BLOCK == VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x8_UNORM_BLOCK == VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x8_SRGB_BLOCK == VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x5_UNORM_BLOCK == VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x5_SRGB_BLOCK == VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x6_UNORM_BLOCK == VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x6_SRGB_BLOCK == VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x8_UNORM_BLOCK == VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x8_SRGB_BLOCK == VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x10_UNORM_BLOCK == VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x10_SRGB_BLOCK == VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x10_UNORM_BLOCK == VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x10_SRGB_BLOCK == VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x12_UNORM_BLOCK == VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x12_SRGB_BLOCK == VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    static_assert((u32)assetlib::ImageFormat::GBGR8_422_UNORM == VK_FORMAT_G8B8G8R8_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::B8G8RG8_422_UNORM == VK_FORMAT_B8G8R8G8_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_420_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8R8_2PLANE_420_UNORM == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_422_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8R8_2PLANE_422_UNORM == VK_FORMAT_G8_B8R8_2PLANE_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8_R8_3PLANE_444_UNORM == VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
    static_assert((u32)assetlib::ImageFormat::R10X6_UNORM_PACK16 == VK_FORMAT_R10X6_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::R10X6G10X6_UNORM_2PACK16 == VK_FORMAT_R10X6G10X6_UNORM_2PACK16);
    static_assert((u32)assetlib::ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16 == VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 == VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 == VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)assetlib::ImageFormat::R12X4_UNORM_PACK16 == VK_FORMAT_R12X4_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::R12X4G12X4_UNORM_2PACK16 == VK_FORMAT_R12X4G12X4_UNORM_2PACK16);
    static_assert((u32)assetlib::ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16 == VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 == VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 == VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
    static_assert((u32)assetlib::ImageFormat::G16B16G16R16_422_UNORM == VK_FORMAT_G16B16G16R16_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::B16G16RG16_422_UNORM == VK_FORMAT_B16G16R16G16_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_420_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
    static_assert((u32)assetlib::ImageFormat::G16_B16R16_2PLANE_420_UNORM == VK_FORMAT_G16_B16R16_2PLANE_420_UNORM);
    static_assert((u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_422_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G16_B16R16_2PLANE_422_UNORM == VK_FORMAT_G16_B16R16_2PLANE_422_UNORM);
    static_assert((u32)assetlib::ImageFormat::G16_B16_R16_3PLANE_444_UNORM == VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
    static_assert((u32)assetlib::ImageFormat::G8_B8R8_2PLANE_444_UNORM == VK_FORMAT_G8_B8R8_2PLANE_444_UNORM);
    static_assert(
        (u32)assetlib::ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16);
    static_assert(
        (u32)assetlib::ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16 ==
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16);
    static_assert((u32)assetlib::ImageFormat::G16_B16R16_2PLANE_444_UNORM == VK_FORMAT_G16_B16R16_2PLANE_444_UNORM);
    static_assert((u32)assetlib::ImageFormat::A4RGB4_UNORM_PACK16 == VK_FORMAT_A4R4G4B4_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::A4B4G4R4_UNORM_PACK16 == VK_FORMAT_A4B4G4R4_UNORM_PACK16);
    static_assert((u32)assetlib::ImageFormat::ASTC_4x4_FLOAT_BLOCK == VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x4_FLOAT_BLOCK == VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_5x5_FLOAT_BLOCK == VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x5_FLOAT_BLOCK == VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_6x6_FLOAT_BLOCK == VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x5_FLOAT_BLOCK == VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x6_FLOAT_BLOCK == VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_8x8_FLOAT_BLOCK == VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x5_FLOAT_BLOCK == VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x6_FLOAT_BLOCK == VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x8_FLOAT_BLOCK == VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_10x10_FLOAT_BLOCK == VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x10_FLOAT_BLOCK == VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::ASTC_12x12_FLOAT_BLOCK == VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK);
    static_assert((u32)assetlib::ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG == VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG == VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
    static_assert((u32)assetlib::ImageFormat::RG16_SFIXED5_NV == VK_FORMAT_R16G16_SFIXED5_NV);

    return (VkFormat)(u32)format;
}

IoResult<assetlib::ImageAsset> readUncompressedHdr(const assetlib::ImageMeta& meta)
{
    i32 width, height, channels;
    u8* pixels{nullptr};
    pixels = (u8*)stbi_loadf(meta.Metadata.Io.OriginalFile.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    const u64 sizeBytes = (u64)width * (u64)height * 4llu * sizeof(f32);
    std::vector imageData((std::byte*)pixels, (std::byte*)pixels + sizeBytes);
    stbi_image_free(pixels);

    return assetlib::ImageAsset{
        .Header = {
            .Format = meta.BakedFormat,
            .Kind = meta.IsCubemap ? assetlib::ImageKind::ImageCubemap : assetlib::ImageKind::Image2d,
            .Width = (u32)width,
            .Height = (u32)height,
            .Layers = 1,
            .Mipmaps = 1,
            .GenerateMipmaps = meta.RuntimeMipmaps && !meta.PregeneratedMipmaps,
            .MipmapSizes = {{(u32)sizeBytes}}
        },
        .MipmapsImageData = {{{std::move(imageData)}}}
    };
}

IoResult<assetlib::ImageAsset> readUncompressedLdr(const assetlib::ImageMeta& meta)
{
    i32 width, height, channels;
    u8* pixels{nullptr};
    pixels = (u8*)stbi_load(meta.Metadata.Io.OriginalFile.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    const u64 sizeBytes = (u64)width * (u64)height * 4llu;
    std::vector imageData((std::byte*)pixels, (std::byte*)pixels + sizeBytes);
    stbi_image_free(pixels);

    return assetlib::ImageAsset{
        .Header = {
            .Format = meta.BakedFormat,
            .Kind = meta.IsCubemap ? assetlib::ImageKind::ImageCubemap : assetlib::ImageKind::Image2d,
            .Width = (u32)width,
            .Height = (u32)height,
            .Layers = 1,
            .Mipmaps = 1,
            .GenerateMipmaps = meta.RuntimeMipmaps && !meta.PregeneratedMipmaps,
            .MipmapSizes = {{(u32)sizeBytes}}
        },
        .MipmapsImageData = {{{std::move(imageData)}}}
    };
}
}

IoResult<assetlib::ImageAsset> ImageBaker::BakeHDR(const assetlib::ImageMeta& meta)
{
    CHECK_RETURN_IO_ERROR(!meta.PregeneratedMipmaps, IoError::ErrorCode::GeneralError,
        "Pregenerated mipmaps are currently not supported for hdr images ({})", meta.Metadata.Io.OriginalFile)
    CHECK_RETURN_IO_ERROR(!meta.IsCubemap, IoError::ErrorCode::GeneralError,
        "Cubemaps are currently not supported for hdr images ({})", meta.Metadata.Io.OriginalFile)

    std::vector<assetlib::ImageAsset::LayerImageData> imageData;

    CHECK_RETURN_IO_ERROR(meta.BakedFormat == assetlib::ImageFormat::RGBA32_FLOAT,
        IoError::ErrorCode::GeneralError,
        "Only uncompressed hdr format (RGBA32_FLOAT) is supported ({})", meta.Metadata.Io.OriginalFile)
    
    return readUncompressedHdr(meta);
}

IoResult<assetlib::ImageAsset> ImageBaker::BakeLDRKtx(const assetlib::ImageMeta& meta)
{
    ktx::CreateCommandOptions options = {};
    options.inputFilepaths = {meta.Metadata.Io.OriginalFile};
    options.vkFormat = vkFormatFromImageFormat(meta.BakedFormat);
    options.cubemap = meta.IsCubemap;
    options.mipmapGenerate = meta.PregeneratedMipmaps;
    options.defaultMipmapWrap = basisu::Resampler::BOUNDARY_CLAMP;
    options.codec = ktx::BasisCodec::NONE;
    options.encodeASTC = false;
    
    ktx::CommandCreate create(options);
    const ktx::KTXTexture2 texture = create.execute();
    CHECK_RETURN_IO_ERROR(texture.handle() != nullptr, IoError::ErrorCode::GeneralError,
        "Failed to create image ({})", meta.Metadata.Io.OriginalFile)

    assetlib::ImageAsset imageAsset = {
        .Header = {
            .Format = meta.BakedFormat,
            .Kind = meta.IsCubemap ? assetlib::ImageKind::ImageCubemap : assetlib::ImageKind::Image2d,
            .Width = texture->baseWidth,
            .Height = texture->baseHeight,
            .Depth = texture->baseDepth,
            .Layers = texture->numFaces,
            .Mipmaps = texture->numLevels,
            .GenerateMipmaps = meta.RuntimeMipmaps && !meta.PregeneratedMipmaps,
        }
    };
    imageAsset.Header.MipmapSizes.resize(texture->numLevels);
    imageAsset.MipmapsImageData.resize(texture->numLevels);
    for (auto& mip : imageAsset.Header.MipmapSizes)
        mip.resize(texture->numFaces);
    for (auto& mip : imageAsset.MipmapsImageData)
        mip.resize(texture->numFaces);
    
    const KTX_error_code error = ktxTexture2_IterateLevels(texture.handle(), [](
        i32 mip, i32 level,
        i32, i32, i32,
        u64 pixelsSizeBytes, void* pixels, void* userdata) -> KTX_error_code {

        assetlib::ImageAsset& image = *(assetlib::ImageAsset*)userdata;
        image.Header.MipmapSizes[mip][level] = (u32)pixelsSizeBytes;
        image.MipmapsImageData[mip][level] = std::vector((std::byte*)pixels, (std::byte*)pixels + pixelsSizeBytes);
        
        return KTX_SUCCESS;
    }, &imageAsset);
    CHECK_RETURN_IO_ERROR(error == KTX_SUCCESS, IoError::ErrorCode::GeneralError,
        "Failed to get image data ({})", meta.Metadata.Io.OriginalFile)

    return imageAsset;
}

IoResult<assetlib::ImageAsset> ImageBaker::BakeLDRJpg(const assetlib::ImageMeta& meta)
{
    CHECK_RETURN_IO_ERROR(!meta.PregeneratedMipmaps, IoError::ErrorCode::GeneralError,
        "Pregenerated mipmaps for .jpg are not supported")
    CHECK_RETURN_IO_ERROR(
        meta.BakedFormat == assetlib::ImageFormat::RGBA8_SRGB ||
        meta.BakedFormat == assetlib::ImageFormat::RGBA8_UNORM
        , IoError::ErrorCode::GeneralError,
        "Only uncompressed image formats are supported for .jpg are")
    
    return readUncompressedLdr(meta);
}
}