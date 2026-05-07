#include "rendererpch.h"
#include "AssetSystem.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <CoreLib/Platform/FileWatcher.h>

#include <utility>

namespace fs = std::filesystem;

namespace lux
{
void AssetSystem::Init(const std::shared_ptr<import::Context>& context)
{
    m_Ctx = context;
    m_ImportQueue.Init();
}

void AssetSystem::Shutdown()
{
    m_ImportQueue.Shutdown();
}

void AssetSystem::RegisterAssetManager(assetlib::AssetType type, AssetManager& manager)
{
    m_Managers[type] = &manager;
}

void AssetSystem::SetAssetsDirectory(const std::filesystem::path& path)
{
    m_AssetsDirectory = path;
    
    InitFileWatcher(path);
}

void AssetSystem::ScanAssetsDirectory()
{
    for (auto& manager : m_Managers | std::views::values)
        manager->OnAssetSystemInit();
    
    ScanAssetsDirectory(m_AssetsDirectory);
}

void AssetSystem::ScanAssetsDirectory(const std::filesystem::path& path)
{
    for (const auto& file : fs::recursive_directory_iterator(path))
    {
        if (file.is_directory())
            continue;
        
        const std::filesystem::path filePath = std::filesystem::weakly_canonical(file.path()).generic_string();
        if (filePath.extension() != assetlib::ASSETLIB_METADATA_EXTENSION)
            continue;
            
        auto metadataRead = assetlib::io::readBaseAssetMetadata(filePath);
        if (!metadataRead.has_value())
            continue;

        for (auto& manager : m_Managers | std::views::values)
            if (manager->AddManaged(*metadataRead, filePath))
                m_IdResolver.RegisterId(metadataRead->AssetId, {
                    .Path = metadataRead->Io.OriginalFile,
                    .MetaPath = filePath,
                    .AssetType = metadataRead->Type.Type
                });
    }
}

void AssetSystem::SubscribeOnAssetUpdate(assetlib::AssetType type, AssetUpdatedHandler& handler)
{
    handler.Connect(m_AssetUpdatedSignals[type]);
}

void AssetSystem::NotifyAssetUpdate(assetlib::AssetType type, const AssetUpdatedInfo& assetInfo)
{
    m_AssetUpdatedSignals[type].Emit(assetInfo);
}

const AssetIdResolver::AssetInfo* AssetSystem::Resolve(assetlib::AssetId id) const
{
    return m_IdResolver.Resolve(id);
}

assetlib::AssetId AssetSystem::ResolveMetaPath(const std::filesystem::path& path) const
{
    return m_IdResolver.ResolveMetaPath(path);
}

bool AssetSystem::AddImportRequest(AssetImportRequest&& request)
{
    return m_ImportQueue.AddRequest(std::move(request));
}

void AssetSystem::InitFileWatcher(const std::filesystem::path& path)
{
    m_FileWatcher = std::make_unique<FileWatcher>();
    if (const auto res = m_FileWatcher->Watch(path); !res.has_value())
    {
        LUX_LOG_ERROR("Failed to init file watcher for directory {}. Error: {}",
            path.string(), FileWatcher::ErrorDescription(res.error()));
        return;
    }

    m_FileWatcherHandler = ::FileWatcherHandler([this](const FileWatcherEvent& event)
    {
        const std::filesystem::path filePath = event.Name;

        if (event.Action != FileWatcherEvent::ActionType::Modify)
            return;
        
        /* is it possible that file is deleted or renamed before we begin to process it */
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath))
            return;

        for (auto& manager : m_Managers | std::views::values)
            manager->OnFileModified(filePath);
    });

    if (const auto res = m_FileWatcher->Subscribe(m_FileWatcherHandler); !res.has_value())
        LUX_LOG_ERROR("Failed to subscribe to file watcher events for directory {}. Error: {}",
        path.string(), FileWatcher::ErrorDescription(res.error()));
}
}
