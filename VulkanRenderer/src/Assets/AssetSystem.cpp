#include "rendererpch.h"
#include "AssetSystem.h"

#include "AssetManager.h"
#include "cvars/CVarSystem.h"
#include "Platform/FileWatcher.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"

#include <utility>

namespace fs = std::filesystem;

namespace lux
{
void AssetSystem::Init(assetlib::io::AssetIoInterface& io, assetlib::io::AssetCompressor& compressor)
{
    m_Io = &io;
    m_Compressor = &compressor;

    m_Bakery.Init();
}

void AssetSystem::Shutdown()
{
    m_Bakery.Shutdown();
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
    for (const auto& file : fs::recursive_directory_iterator(m_AssetsDirectory))
    {
        if (file.is_directory())
            continue;

        for (auto& manager : m_Managers | std::views::values)
            manager->AddManaged(file, m_IdResolver);
    }
}

const AssetIdResolver::AssetInfo* AssetSystem::Resolve(assetlib::AssetId id) const
{
    return m_IdResolver.Resolve(id);
}

bool AssetSystem::AddBakeRequest(AssetBakeRequest&& request)
{
    return m_Bakery.AddRequest(std::move(request));
}

void AssetSystem::InitFileWatcher(const std::filesystem::path& path)
{
    m_FileWatcher = std::make_unique<FileWatcher>();
    if (const auto res = m_FileWatcher->Watch(path); !res.has_value())
    {
        LOG("Failed to init file watcher for directory {}. Error: {}",
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
        LOG("Failed to subscribe to file watcher events for directory {}. Error: {}",
        path.string(), FileWatcher::ErrorDescription(res.error()));
}
}
