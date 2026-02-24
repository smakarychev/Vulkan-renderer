#include "rendererpch.h"
#include "ImageAssetManager.h"

#include "FrameContext.h"
#include "Assets/AssetIdResolver.h"
#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Bakers/Images/ImageBaker.h"
#include "Rendering/DeletionQueue.h"
#include "Rendering/Image/ImageUtility.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"
#include "Vulkan/Device.h"

namespace lux
{
bool ImageAssetManager::AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    if (path.extension() != bakers::IMAGE_ASSET_EXTENSION)
        return false;

    auto assetFile = m_Context.Io->ReadHeader(path);
    if (!assetFile.has_value())
        return false;

    auto imageHeader = assetlib::image::readHeader(*assetFile);
    if (!imageHeader.has_value())
        return false;

    resolver.RegisterId(assetFile->Metadata.AssetId, {
        .Path = path,
        .AssetType = assetFile->Metadata.Type
    });

    return true;
}

bool ImageAssetManager::Bakes(const std::filesystem::path& path)
{
    bool bakes = false;
    for (auto& extension : bakers::IMAGE_ASSET_RAW_EXTENSIONS)
        bakes = bakes || path.extension() == extension;

    return bakes || path.extension() == bakers::ImageBaker::IMAGE_LOAD_INFO_EXTENSION;
}

void ImageAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Bakes(path))
        OnRawFileModified(path);
}

void ImageAssetManager::Init(const bakers::ImageBakeSettings& bakeSettings)
{
    m_Context = {
        .InitialDirectory = m_AssetSystem->GetAssetsDirectory(),
        .Io = &m_AssetSystem->GetIo(),
        .Compressor = &m_AssetSystem->GetCompressor()
    };

    m_BakeSettings = &bakeSettings;
}

void ImageAssetManager::Shutdown()
{
    for (ImageHandle handle : m_ImagesMap | std::views::values |
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
    auto it = m_ImagesMap.find(path);
    if (it != m_ImagesMap.end())
        return it->second;

    const Image image = DoLoad(parameters);
    const u32 imageIndex = m_Images.add(image);
    const ImageHandle handle(imageIndex, 0);
    m_ImagesMap.emplace(path, handle);
    if (m_HandlesToPaths.size() <= handle.Index())
        m_HandlesToPaths.resize(handle.Index() + 1);
    m_HandlesToPaths[handle.Index()] = path;

    return handle;
}

void ImageAssetManager::UnloadAsset(ImageHandle handle)
{
    ASSERT(m_FrameDeletionQueue)

    if (handle.Index() >= m_HandlesToPaths.size())
        return;

    const auto& path = m_HandlesToPaths[handle.Index()];
    if (path.empty())
        return;

    m_FrameDeletionQueue->Enqueue(GetAsset(handle));
    m_ImagesMap.erase(path);
    m_HandlesToPaths[handle.Index()] = std::filesystem::path{};
    m_Images.remove(handle.Index());
}

Image ImageAssetManager::GetAsset(ImageHandle handle) const
{
    return m_Images[handle.Index()];
}

void ImageAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    std::filesystem::path loadPath = path;
    if (path.extension() != bakers::ImageBaker::IMAGE_LOAD_INFO_EXTENSION)
        loadPath.replace_extension(bakers::ImageBaker::IMAGE_LOAD_INFO_EXTENSION);

    m_AssetSystem->AddBakeRequest({
        .BakeFn = [this, loadPath]()
        {
            bakers::ImageBaker baker;
            auto bakedPath = baker.BakeToFile(loadPath, *m_BakeSettings, m_Context);
            if (!bakedPath)
            {
                LOG("Warning: Bake request failed {} ({})", bakedPath.error(), loadPath.string());
                return;
            }

            OnBakedFileModified(*bakedPath);
        }
    });
}

void ImageAssetManager::OnBakedFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);

    auto it = m_ImagesMap.find(weakly_canonical(path).generic_string());
    if (it == m_ImagesMap.end())
        return;

    m_FrameDeletionQueue->Enqueue(GetAsset(it->second));
    const Image newImage = DoLoad({.Path = path});
    if (newImage.HasValue())
        m_Images[it->second.Index()] = newImage;
}

Image ImageAssetManager::DoLoad(const ImageLoadParameters& parameters) const
{
    const auto assetFile = m_Context.Io->ReadHeader(parameters.Path);
    if (!assetFile.has_value())
        return {};

    auto imageAsset = assetlib::image::readImage(*assetFile, *m_Context.Io, *m_Context.Compressor);
    if (!imageAsset.has_value())
        return {};

    u32 layersDepth = imageAsset->Header.Layers;
    i8 mips = (i8)imageAsset->Header.Mipmaps;
    if (imageAsset->Header.Kind == assetlib::ImageKind::Image3d)
        layersDepth = imageAsset->Header.Depth;
    if (imageAsset->Header.GenerateMipmaps)
        mips = imageAsset->Header.Kind == assetlib::ImageKind::Image3d ?
           Images::mipmapCount({imageAsset->Header.Width, imageAsset->Header.Height, imageAsset->Header.Depth}) :
           Images::mipmapCount({imageAsset->Header.Width, imageAsset->Header.Height});

    const Image image = Device::CreateImage({
        .DataSource = &(*imageAsset),
        .Description = ImageDescription{
            .Width = imageAsset->Header.Width,
            .Height = imageAsset->Header.Height,
            .LayersDepth = layersDepth,
            .Mipmaps = mips,
            .Format = formatFromAssetImageFormat(imageAsset->Header.Format),
            .Kind = imageKindFromAssetImageKind(imageAsset->Header.Kind),
            .Usage = ImageUsage::Sampled,
        },
        .CalculateMipmaps = imageAsset->Header.GenerateMipmaps
    }, Device::DummyDeletionQueue());

    return image;
}
}
