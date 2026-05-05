#include "ImageImporter.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Importers/Import.h>
#include <CoreLib/Utils/FileUtils.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

namespace lux::import
{
ImageImporter::ImageImporter(const std::shared_ptr<Context>& ctx, const MetadataHint& metadataHint)
    : Importer(ctx), m_Baker(ctx), m_MetadataHint(metadataHint)
{
}

ImportResult<void> ImageImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    CHECK_RETURN_IMPORT_ERROR(enumHasAny(importFlags, ImportFlags::Binaries | ImportFlags::Header),
        IoError::ErrorCode::GeneralError, "ImageImporter error flags do not include neither header nor binaries {}",
        path.string())
    
    const std::filesystem::path importPath = GetMetaPath(path);
    
    if (!std::filesystem::exists(importPath))
    {
        auto writeResult = WriteMetadata(importPath, path);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }

    auto metadataRead = assetlib::image::readMeta(importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    if (m_Baker.ShouldBake(importPath))
    {
        if (!enumHasAny(importFlags, ImportFlags::BakeIfNotBaked))
            return std::unexpected(ImportError{
                {
                    .Code = IoError::ErrorCode::GeneralError,
                    .Message = std::format("Failed to import image: {}", path.string())
                },
                ImportError::ImportErrorCode::NotBaked
            });
        
        auto bakeResult = m_Baker.BakeToFile(m_ImportedMeta, importPath);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(bakeResult)
    }
    
    if (enumHasAny(importFlags, ImportFlags::Header))
    {
        auto headerRead = assetlib::image::readHeader(m_ImportedMeta.Metadata);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(headerRead)
        m_ImportedAsset.Asset.Header = std::move(*headerRead);
    }
    if (enumHasAny(importFlags, ImportFlags::Binaries))
    {
        const auto& header = m_ImportedAsset.Asset.Header;
        m_ImportedAsset.Asset.MipmapsImageData.resize(header.Mipmaps);
        for (auto& mip : m_ImportedAsset.Asset.MipmapsImageData)
            mip.resize(header.Layers);
    
        for (u32 mip = 0; mip < header.Mipmaps; mip++)
        {
            for (u32 layer = 0; layer < header.Layers; layer++)
            {
                auto binariesRead = assetlib::image::readImageData(header, m_ImportedMeta.Metadata, mip, layer,
                    *m_Ctx->Io, *m_Ctx->Compressor);
                CHECK_RETURN_IMPORT_ERROR_PROPAGATE(binariesRead)
                m_ImportedAsset.Asset.MipmapsImageData[mip][layer] = std::move(*binariesRead);
            }
        }
    }
        
    return {};
}

namespace 
{
assetlib::ImageFormat chooseImageFormat(const std::filesystem::path& path, 
    ImageImporter::MetadataHint::NameHeuristics heuristics)
{
    if (heuristics != ImageImporter::MetadataHint::NameHeuristics::Gltf)
        return assetlib::ImageFormat::Undefined;

    std::string filename = path.filename().string();
    std::ranges::transform(filename, filename.begin(), [](char c){ return std::tolower(c); });

    if (filename.contains("_basecolor") ||
        filename.contains("_emissive"))
        return assetlib::ImageFormat::RGBA8_SRGB;
    if (filename.contains("_norm") ||
        filename.contains("_rough") ||
        filename.contains("_metal") ||
        filename.contains("_occlusion"))
        return assetlib::ImageFormat::RGBA8_UNORM;

    return assetlib::ImageFormat::RGBA8_SRGB;
}
}

bool ImageImporter::NeedsBaking(const std::filesystem::path& path) const
{
    return m_Baker.ShouldBake(GetMetaPath(path));
}

std::filesystem::path ImageImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

IoResult<void> ImageImporter::WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath)
{
    if (std::filesystem::exists(metaPath) && !m_MetadataHint.Overwrite)
        return {};
    
    const bool isHrdImage = rawPath.extension() == IMAGE_ASSET_RAW_HDR_EXTENSION;
    assetlib::ImageFormat format = isHrdImage ?
        assetlib::ImageFormat::RGBA32_FLOAT : assetlib::ImageFormat::RGBA8_SRGB;
    if (m_MetadataHint.BakedFormat != assetlib::ImageFormat::Undefined)
    {
        format = m_MetadataHint.BakedFormat;
    }
    else if (m_MetadataHint.Heuristics != MetadataHint::NameHeuristics::None && !isHrdImage)
    {
        const assetlib::ImageFormat inferred = chooseImageFormat(rawPath, m_MetadataHint.Heuristics);
        format = inferred != assetlib::ImageFormat::Undefined ? inferred : format;
    }
    const assetlib::ImageMeta imageMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::image::getTypeMetadata()),
        .PregeneratedMipmaps =
            rawPath.extension() != IMAGE_ASSET_RAW_HDR_EXTENSION &&
            rawPath.extension() != IMAGE_ASSET_RAW_JPG_EXTENSION &&
            rawPath.extension() != IMAGE_ASSET_RAW_JPEG_EXTENSION,
        .BakedFormat = format
    };
    
    return WritePackedMetadata(metaPath, assetlib::image::packMeta(imageMeta), "image");
}
}
