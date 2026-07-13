#include "rendererpch.h"
#include "ImageAssetManager.h"

#include "FrameContext.h"
#include "Assets/AssetIdResolver.h"
#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Rendering/DeletionQueue.h"
#include "Rendering/Image/ImageUtility.h"
#include "Vulkan/Device.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetImportLib/Importers/Images/ImageImporter.h>

namespace lux
{
bool ImageAssetManager::AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path&)
{
    return metadata.Type.Type == assetlib::image::ASSET_TYPE;
}

bool ImageAssetManager::Imports(std::string_view extension)
{
    bool bakes = false;
    for (auto& rawExtension : import::IMAGE_ASSET_RAW_EXTENSIONS)
        bakes = bakes || extension == rawExtension;

    return bakes;
}

void ImageAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Imports(path.extension().string()) || Imports(assetlib::getMetadataRawExtension(path)))
        OnRawFileModified(path);
}

void ImageAssetManager::Shutdown()
{
    for (ImageHandle handle : m_Images | std::views::values |
         std::views::filter([this](const ImageHandle handle) { return GetAsset(handle).HasValue(); }))
        Device::Destroy(GetAsset(handle));
}

void ImageAssetManager::OnFrameBegin(FrameContext& ctx)
{
    m_FrameDeletionQueue = &ctx.DeletionQueue;
}

ImageHandle ImageAssetManager::LoadAsset(const ImageLoadParameters& parameters)
{
    const std::filesystem::path path = weakly_canonical(parameters.Path).generic_string();
    import::ImageImporter importer(m_Ctx, {});
    const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));
    
    const ImageHandle cached = m_Images.Find(id);
    if (cached.IsValid())
        return cached;

    return m_Images.Add(DoLoad(importer, path), id);
}

void ImageAssetManager::UnloadAsset(ImageHandle handle)
{
    ASSERT(m_FrameDeletionQueue)

    const assetlib::AssetId id = m_Images.Find(handle);
    if (!id.HasValue())
        return;

    const auto* assetInfo = m_AssetSystem->Resolve(id);
    if (!assetInfo)
        return;

    LUX_LOG_INFO("Unloading image: {}", assetInfo->Path.string());

    m_FrameDeletionQueue->Enqueue(GetAsset(handle));
    m_Images.Erase(handle, id);
}

Image ImageAssetManager::GetAsset(ImageHandle handle) const
{
    return m_Images[handle.Index()];
}

void ImageAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    m_AssetSystem->AddImportRequest({
        .ImportFn = [this, path]()
        {
            import::ImageImporter importer(m_Ctx, {});
            AssetSystemFileLockGuard fileLock = m_AssetSystem->LockAssetFile(path, importer);
            
            const ImageAsset newImage = DoLoad(importer, path);
            if (!newImage.HasValue())
                return;

            const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));
            ImageHandle cached;
            {
                Lock lock(m_ResourceAccessMutex);
                cached = m_Images.Find(id);

                /* new image was created */
                if (!cached.IsValid())
                {
                    m_AssetSystem->RegisterAsset(importer.GetMetaPath(path), importer.GetImportedAssetMetadata());
                    return;
                }

                m_FrameDeletionQueue->Enqueue(GetAsset(cached));
                m_Images[cached.Index()] = newImage;
            }

            m_AssetSystem->NotifyAssetUpdate(assetlib::image::ASSET_TYPE, {.AssetHandle = cached});
        }
    });
}

ImageAsset ImageAssetManager::DoLoad(import::ImageImporter& importer, const std::filesystem::path& path) const
{
    LUX_LOG_INFO("Loading image: {}", path.string());
    
    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load image: {} ({})", imported.error(), path.string());
        return {};
    }
    auto& imageAsset = importer.GetImportedImage().Asset;

    u32 layersDepth = imageAsset.Header.Layers;
    i8 mips = (i8)imageAsset.Header.Mipmaps;
    if (imageAsset.Header.Kind == assetlib::ImageKind::Image3d)
        layersDepth = imageAsset.Header.Depth;
    if (imageAsset.Header.GenerateMipmaps)
        mips = imageAsset.Header.Kind == assetlib::ImageKind::Image3d ?
           Images::mipmapCount({imageAsset.Header.Width, imageAsset.Header.Height, imageAsset.Header.Depth}) :
           Images::mipmapCount({imageAsset.Header.Width, imageAsset.Header.Height});

    const Image image = Device::CreateImage({
        .DataSource = &imageAsset,
        .Description = ImageDescription{
            .Width = imageAsset.Header.Width,
            .Height = imageAsset.Header.Height,
            .LayersDepth = layersDepth,
            .Mipmaps = mips,
            .Format = formatFromAssetImageFormat(imageAsset.Header.Format),
            .Kind = imageKindFromAssetImageKind(imageAsset.Header.Kind),
            .Usage = ImageUsage::Sampled,
        },
        .CalculateMipmaps = imageAsset.Header.GenerateMipmaps
    }, Device::DummyDeletionQueue());

    return image;
}
}
