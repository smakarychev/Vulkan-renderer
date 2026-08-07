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
    IterateCacheUnsafe([this](const auto& _, ImageHandle handle)
    {
        const Image image = GetAsset(handle);
        if (!image.HasValue())
            return;
        
        Device::Destroy(image);
    });
}

void ImageAssetManager::OnFrameBegin(FrameContext& ctx)
{
    m_FrameDeletionQueue = ctx.DeletionQueue.get();
}

ImageHandle ImageAssetManager::LoadAsset(const ImageLoadParameters& parameters)
{
    import::ImageImporter importer(m_Ctx, {});
    
    return m_Images.Insert(DoLoad(importer, parameters.Path));
}

void ImageAssetManager::ReloadAsset(ImageHandle handle)
{
    const auto* parameters = FindLoadParametersUnsafe(handle);
    if (!parameters)
        return;
    
    import::ImageImporter importer(m_Ctx, {});
    m_AssetSystem->SkipFileUpdateNotify(importer.GetMetaPath(parameters->Path));
            
    const ImageAsset newImage = DoLoad(importer, parameters->Path);
    if (!newImage.HasValue())
        return;
    
    m_FrameDeletionQueue->Enqueue(GetAsset(handle));
    m_Images[handle.Index()] = newImage;

    m_AssetSystem->NotifyAssetUpdate({.Type =assetlib::image::ASSET_TYPE, .AssetHandle = handle});
}

void ImageAssetManager::UnloadAsset(ImageHandle handle)
{
    ASSERT(m_FrameDeletionQueue)
    
    const auto* parameters = FindLoadParametersUnsafe(handle);
    if (!parameters)
        return;
    
    LUX_LOG_INFO("Unloading image: {}", parameters->Path.ToString());

    m_FrameDeletionQueue->Enqueue(GetAsset(handle));
    m_Images.Erase(handle);
}

Image ImageAssetManager::GetAsset(ImageHandle handle) const
{
    return m_Images[handle.Index()];
}

void ImageAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);
    
    const ImageHandle handle = FindHandleByLoadParametersUnsafe({.Path = path});
    /* new image was created */
    if (!handle.IsValid())
    {
        m_AssetSystem->RegisterAsset(import::ImageImporter(m_Ctx, {}).GetMetaPath(path));
        return;
    }
    
    m_AssetSystem->AddImportRequest({
        .ImportFn = [this, handle]()
        {
            Lock lock(m_ResourceAccessMutex);
            ReloadAsset(handle);
        }
    });
}

ImageAsset ImageAssetManager::DoLoad(import::ImageImporter& importer, const AssetPath& path) const
{
    LUX_LOG_INFO("Loading image: {}", path.ToString());
    
    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load image: {} ({})", imported.error(), path.ToString());
        return {};
    }
    auto& imageAsset = importer.GetImportedImage().Asset;

    u32 layersDepth = imageAsset.Header.Layers;
    i8 mips = (i8)imageAsset.Header.Mipmaps;
    if (imageAsset.Header.Kind == assetlib::ImageKind::Image3d)
        layersDepth = imageAsset.Header.Depth;
    if (imageAsset.Header.GenerateMipmaps)
        mips = imageAsset.Header.Kind == assetlib::ImageKind::Image3d ?
           images::mipmapCount({imageAsset.Header.Width, imageAsset.Header.Height, imageAsset.Header.Depth}) :
           images::mipmapCount({imageAsset.Header.Width, imageAsset.Header.Height});

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
