#pragma once
#include "AssetImportQueue.h"
#include "AssetIdResolver.h"
#include "AssetManager.h"

#include <CoreLib/Platform/FileWatcher.h>

namespace lux
{
namespace import
{
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
    AssetHandleBase AssetHandle{};
};
using AssetUpdatedSignal = Signal<AssetUpdatedInfo>;
using AssetUpdatedHandler = SignalHandler<AssetUpdatedInfo>;

class AssetSystem
{
public:
    void Init(const std::shared_ptr<import::Context>& context);
    void Shutdown();
    void RegisterAssetManager(assetlib::AssetType type, AssetManager& manager);
    void SetAssetsDirectory(const std::filesystem::path& path);
    void ScanAssetsDirectory();
    void ScanAssetsDirectory(const std::filesystem::path& path);
    
    void SubscribeOnAssetUpdate(assetlib::AssetType type, AssetUpdatedHandler& handler);
    void NotifyAssetUpdate(assetlib::AssetType type, const AssetUpdatedInfo& assetInfo);

    template <typename ResourceAssetManager>
    requires std::is_base_of_v<AssetManager, ResourceAssetManager>
    ResourceAssetManager* GetAssetManagerFor(assetlib::AssetType type);
    
    const AssetIdResolver::AssetInfo* Resolve(assetlib::AssetId id) const;
    assetlib::AssetId ResolveMetaPath(const std::filesystem::path& path) const;
    bool AddImportRequest(AssetImportRequest&& request);

    const std::filesystem::path& GetAssetsDirectory() const { return m_AssetsDirectory; }
    const std::shared_ptr<import::Context>& GetContext() const { return m_Ctx; }
private:
    void InitFileWatcher(const std::filesystem::path& path);
private:
    std::unordered_map<assetlib::AssetType, AssetManager*> m_Managers;

    AssetIdResolver m_IdResolver;
    std::unique_ptr<FileWatcher> m_FileWatcher;
    FileWatcherHandler m_FileWatcherHandler;

    std::filesystem::path m_AssetsDirectory{};
    std::shared_ptr<import::Context> m_Ctx{nullptr};
    
    std::unordered_map<assetlib::AssetType, AssetUpdatedSignal> m_AssetUpdatedSignals;

    AssetImportQueue m_ImportQueue;
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
