#include "rendererpch.h"
#include "AssetSystem.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Importers/Importer.h>
#include <CoreLib/Platform/FileWatcher.h>

#include <utility>

namespace fs = std::filesystem;

namespace lux
{
AssetSystem& AssetSystem::Get()
{
    static AssetSystem assetSystem = {};
    
    return assetSystem;
}

void AssetSystem::Init(const std::shared_ptr<import::Context>& context)
{
    m_Ctx = context;
    m_ImportQueue.Init();
}

void AssetSystem::Shutdown()
{
    m_ImportQueue.Shutdown();
}

void AssetSystem::OnUpdate()
{
    NotifyAssetUpdatedWatchers();
}

void AssetSystem::RegisterAssetManager(assetlib::AssetType type, AssetManager& manager)
{
    m_Managers[type] = &manager;
}

void AssetSystem::SetAssetsDirectory(const std::filesystem::path& path)
{
    m_AssetsDirectory = weakly_canonical(path);
    
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
                RegisterAsset(filePath, *metadataRead);
    }
}

void AssetSystem::RegisterAsset(const std::filesystem::path& metadataPath)
{
    auto metadataRead = assetlib::io::readBaseAssetMetadata(metadataPath);
    if (!metadataRead.has_value())
        return;
    
    RegisterAsset(metadataPath, *metadataRead);
}

void AssetSystem::RegisterAsset(const std::filesystem::path& metadataPath, const assetlib::AssetMetadata& metadata)
{
    m_IdResolver.RegisterId(metadata.AssetId, {
        .Path = AssetPath(std::filesystem::path(metadata.Io.OriginalFile)),
        .MetaPath = metadataPath,
        .AssetType = metadata.Type.Type
    });
}

void AssetSystem::SubscribeOnAssetUpdate(assetlib::AssetType type, AssetUpdatedHandler& handler)
{
    handler.Connect(m_AssetUpdatedSignals[type]);
}

void AssetSystem::NotifyAssetUpdate(const AssetUpdatedInfo& assetInfo)
{
    std::scoped_lock lock(m_Mutex);
    m_AssetUpdates.push_back(assetInfo);
}

void AssetSystem::NotifyAssetUpdatedWatchers()
{
    std::scoped_lock lock(m_Mutex);
    for (auto& update : m_AssetUpdates)
        m_AssetUpdatedSignals[update.Type].Emit(update);
        
    m_AssetUpdates.clear();
}

AssetPath AssetSystem::ResolveAssetPath(std::string_view filename) const
{
    const std::filesystem::path assetPath = m_AssetsDirectory / filename;
    for (const auto& file : fs::directory_iterator(assetPath.parent_path()))
    {
        const auto& filePath = file.path();
        
        if (filePath.filename() != assetPath.filename())
            continue;
        
        return {std::filesystem::path(weakly_canonical(filePath).generic_string())};
    }
    
    return {};
}

const AssetIdResolver::AssetInfo* AssetSystem::Resolve(assetlib::AssetId id) const
{
    return m_IdResolver.Resolve(id);
}

bool AssetSystem::AddImportRequest(AssetImportRequest&& request)
{
    return m_ImportQueue.AddRequest(std::move(request));
}

void AssetSystem::SkipFileUpdateNotify(const std::filesystem::path& path)
{
    std::scoped_lock lock(m_Mutex);
    auto& updateInfo = m_SkippedFileUpdateInfos[path];
    updateInfo.Count += 1;
    updateInfo.LatestTimePoint = std::chrono::high_resolution_clock::now();
}

void AssetSystem::InitFileWatcher(const std::filesystem::path& path)
{
    m_FileWatcher = std::make_unique<FileWatcher>();
    if (const auto res = m_FileWatcher->Watch(path, {}); !res.has_value())
    {
        LUX_LOG_ERROR("Failed to init file watcher for directory {}. Error: {}",
            path.string(), FileWatcher::ErrorDescription(res.error()));
        return;
    }

    m_FileWatcherHandler = ::FileWatcherHandler([this](const FileWatcherEvent& event)
    {
        const std::filesystem::path filePath = event.Name;
        
        /* delete expired skipped file updates */
        {
            std::scoped_lock lock(m_Mutex);
            std::erase_if(m_SkippedFileUpdateInfos, [&event](const auto& item) {
                return item.second.LatestTimePoint + SkippedFileUpdateInfo::EXPIRE_TIME < event.TimePoint;
            });   
        }
        
        /* is it possible that file is deleted or renamed before we begin to process it */
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath))
            return;

        if (event.Action == FileWatcherEvent::ActionType::Modify || 
            event.Action == FileWatcherEvent::ActionType::Create || 
            event.Action == FileWatcherEvent::ActionType::Rename)
            OnFileModified(filePath);
    });

    if (const auto res = m_FileWatcher->Subscribe(m_FileWatcherHandler); !res.has_value())
        LUX_LOG_ERROR("Failed to subscribe to file watcher events for directory {}. Error: {}",
        path.string(), FileWatcher::ErrorDescription(res.error()));
}

void AssetSystem::OnFileModified(const std::filesystem::path& path)
{
    {
        std::scoped_lock lock(m_Mutex);
        const auto it = m_SkippedFileUpdateInfos.find(path);
        if (it != m_SkippedFileUpdateInfos.end())
        {
            it->second.Count -= 1;
            if (it->second.Count == 0)
                m_SkippedFileUpdateInfos.erase(it);
            return;
        }
    }
    
    std::filesystem::path converted = path;
    if (converted.extension() == assetlib::ASSETLIB_METADATA_EXTENSION)
        converted.replace_extension();
    
    const AssetPath assetPath = converted;
    
    for (auto& manager : m_Managers | std::views::values)
        manager->OnFileModified(assetPath);
}
}
