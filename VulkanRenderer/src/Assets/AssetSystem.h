#pragma once
#include "AssetImportQueue.h"
#include "AssetIdResolver.h"
#include "AssetManager.h"
#include "AssetPath.h"

#include <CoreLib/Platform/FileWatcher.h>

namespace lux
{
namespace import
{
class Importer;
struct Context;
}

namespace assetlib::io
{
class AssetCompressor;
class AssetIoInterface;
}

class AssetManager;
struct AssetLoadParameters;

struct AssetUpdatedInfo
{
    assetlib::AssetType Type{};
    AssetHandleBase AssetHandle{};
};

using AssetUpdatedSignal = Signal<AssetUpdatedInfo>;
using AssetUpdatedHandler = SignalHandler<AssetUpdatedInfo>;

class AssetSystem
{
public:
    void Init(const std::shared_ptr<import::Context>& context);
    void Shutdown();
    void OnUpdate();
    void RegisterAssetManager(assetlib::AssetType type, AssetManager& manager);
    void SetAssetsDirectory(const std::filesystem::path& path);
    void ScanAssetsDirectory();
    void ScanAssetsDirectory(const std::filesystem::path& path);
    void RegisterAsset(const std::filesystem::path& metadataPath);

    void SubscribeOnAssetUpdate(assetlib::AssetType type, AssetUpdatedHandler& handler);
    void NotifyAssetUpdate(const AssetUpdatedInfo& assetInfo);

    template <typename ResourceAssetManager>
    requires std::is_base_of_v<AssetManager, ResourceAssetManager>
    ResourceAssetManager* GetAssetManagerFor(assetlib::AssetType type);

    const AssetIdResolver::AssetInfo* Resolve(assetlib::AssetId id) const;
    assetlib::AssetId ResolveMetaPath(const std::filesystem::path& path) const;
    bool AddImportRequest(AssetImportRequest&& request);
    void SkipFileUpdateNotify(const std::filesystem::path& path);

    const std::filesystem::path& GetAssetsDirectory() const { return m_AssetsDirectory; }
    const std::shared_ptr<import::Context>& GetContext() const { return m_Ctx; }

private:
    void InitFileWatcher(const std::filesystem::path& path);
    void OnFileModified(const std::filesystem::path& path);
    void RegisterAsset(const std::filesystem::path& metadataPath, const assetlib::AssetMetadata& metadata);
    void NotifyAssetUpdatedWatchers();
private:
    std::unordered_map<assetlib::AssetType, AssetManager*> m_Managers;

    AssetIdResolver m_IdResolver;
    std::unique_ptr<FileWatcher> m_FileWatcher;
    FileWatcherHandler m_FileWatcherHandler;

    std::filesystem::path m_AssetsDirectory{};
    std::shared_ptr<import::Context> m_Ctx{nullptr};

    std::unordered_map<assetlib::AssetType, AssetUpdatedSignal> m_AssetUpdatedSignals;
    std::vector<AssetUpdatedInfo> m_AssetUpdates;
    
    AssetImportQueue m_ImportQueue;
  
    struct SkippedFileUpdateInfo
    {
        static constexpr auto EXPIRE_TIME = 15s;
        u32 Count{0};
        std::chrono::time_point<std::chrono::high_resolution_clock> LatestTimePoint;
    };
    std::unordered_map<std::filesystem::path, SkippedFileUpdateInfo> m_SkippedFileUpdateInfos;
    
    std::mutex m_Mutex;
};

template <typename ResourceAssetManager> requires std::is_base_of_v<AssetManager, ResourceAssetManager>
ResourceAssetManager* AssetSystem::GetAssetManagerFor(assetlib::AssetType type)
{
    auto it = m_Managers.find(type);
    if (it == m_Managers.end())
        return nullptr;

    if (it->second->GetGuid() != ResourceAssetManager::GetGuidStatic())
        return nullptr;

    return (ResourceAssetManager*)it->second;
}
}
