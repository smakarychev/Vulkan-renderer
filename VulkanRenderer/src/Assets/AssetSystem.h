#pragma once
#include "AssetBakery.h"
#include "AssetIdResolver.h"
#include "Platform/FileWatcher.h"

namespace lux
{
namespace assetlib::io
{
class AssetCompressor;
class AssetIoInterface;
}

class AssetManager;
struct AssetLoadParameters;

class AssetSystem
{
public:
    void Init(assetlib::io::AssetIoInterface& io, assetlib::io::AssetCompressor& compressor);
    void Shutdown();
    void RegisterAssetManager(assetlib::AssetType type, AssetManager& manager);
    void SetAssetsDirectory(const std::filesystem::path& path);
    void ScanAssetsDirectory();

    const AssetIdResolver::AssetInfo* Resolve(assetlib::AssetId id) const;
    bool AddBakeRequest(AssetBakeRequest&& request);

    const std::filesystem::path& GetAssetsDirectory() const { return m_AssetsDirectory; }
    assetlib::io::AssetIoInterface& GetIo() const { return *m_Io; }
    assetlib::io::AssetCompressor& GetCompressor() const { return *m_Compressor; }
private:
    void InitFileWatcher(const std::filesystem::path& path);
private:
    std::unordered_map<assetlib::AssetType, AssetManager*> m_Managers;

    AssetIdResolver m_IdResolver;
    std::unique_ptr<FileWatcher> m_FileWatcher;
    FileWatcherHandler m_FileWatcherHandler;

    std::filesystem::path m_AssetsDirectory{};
    assetlib::io::AssetIoInterface* m_Io{nullptr};
    assetlib::io::AssetCompressor* m_Compressor{nullptr};

    AssetBakery m_Bakery;
};
}
