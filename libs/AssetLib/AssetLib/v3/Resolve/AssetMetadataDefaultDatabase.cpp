#include "AssetMetadataDefaultDatabase.h"

#include "AssetPathDefaultResolver.h"

#include <CoreLib/Utils/FileUtils.h>

namespace lux::assetlib
{
AssetMetadataDefaultDatabase::AssetMetadataDefaultDatabase(const std::shared_ptr<IAssetPathResolver>& resolver)
    : m_Resolver(resolver)
{
}

std::optional<AssetPackageMetadata2> AssetMetadataDefaultDatabase::Find(const AssetResolvedPath& resolvedPath) const
{
    const std::scoped_lock lock(m_Mutex);
    auto it = m_PathLookup.find(resolvedPath);
    if (it != m_PathLookup.end())
        return *it->second;
    
    return std::nullopt;
}

std::optional<AssetPackageMetadata2> AssetMetadataDefaultDatabase::Find(const AssetId& packageId) const
{
    const std::scoped_lock lock(m_Mutex);
    auto it = m_Cache.find(packageId);
    if (it != m_Cache.end())
        return it->second;
    
    return std::nullopt;
}

void AssetMetadataDefaultDatabase::ScanDirectory(const std::filesystem::path& directory) const
{
    namespace fs = std::filesystem;
    
    for (const auto& file : fs::recursive_directory_iterator(directory))
    {
        if (file.is_directory())
            continue;
        
        const std::filesystem::path filePath = std::filesystem::weakly_canonical(file.path()).generic_string();
        if (filePath.extension() != ASSETLIB_METADATA_EXTENSION)
            continue;
        
        auto resolvedPath = m_Resolver->Resolve(filePath.string());
        if (!resolvedPath)
        {
            LUX_LOG_WARN(
                "Path {} is a valid .meta file, but it was not registered because asset path did not resolve. "
                "Maybe search path for asset resolver is not set correctly",
                filePath.generic_string());
            continue;
        }
        
        auto metadataFileRead = readFileToString(filePath);
        if (!metadataFileRead)
            continue;
        
        auto metadataParsed = AssetPackageMetadata2::FromString(*metadataFileRead);
        if (!metadataParsed)
            continue;
        
        AddToCaches(std::move(*resolvedPath), std::move(*metadataParsed));
    }
}

void AssetMetadataDefaultDatabase::UpdateMetadata(const std::filesystem::path& metadataPath) const
{
    auto resolvedPath = m_Resolver->Resolve(metadataPath.string());
    if (!resolvedPath)
    {
        LUX_LOG_ERROR("Failed to update metadata, path did not resolve: {}", metadataPath.generic_string());
        return;
    }
    
    auto metadataFileRead = readFileToString(metadataPath);
    if (!metadataFileRead)
    {
        LUX_LOG_ERROR("Failed to update metadata, could not read: {}", metadataPath.generic_string());
        return;
    }
        
    auto metadataParsed = AssetPackageMetadata2::FromString(*metadataFileRead);
    if (!metadataParsed)
    {
        LUX_LOG_ERROR("Failed to update metadata, could not parse: {} ({})", metadataPath.generic_string(),
            metadataParsed.error());
        return;
    }
    
    ReplaceCaches(std::move(*resolvedPath), std::move(*metadataParsed));
}

void AssetMetadataDefaultDatabase::AddToCaches(AssetResolvedPath&& resolvedPath,
    AssetPackageMetadata2&& metadata) const
{
    const std::scoped_lock lock(m_Mutex);
    AssetPackageMetadata2* inserted = &(m_Cache[metadata.PackageId] = std::move(metadata));
    m_PathLookup[std::move(resolvedPath)] = inserted;
}

void AssetMetadataDefaultDatabase::ReplaceCaches(AssetResolvedPath&& resolvedPath,
    AssetPackageMetadata2&& metadata) const
{
    const std::scoped_lock lock(m_Mutex);
    if (auto oldIt = m_PathLookup.find(resolvedPath); oldIt != m_PathLookup.end())
        m_Cache.erase(oldIt->second->PackageId);

    if (m_Cache.contains(metadata.PackageId))
        LUX_LOG_WARN("AssetMetadataDefaultDatabase overwriting metadata for package {}", metadata.PackageId);
    
    AssetPackageMetadata2* inserted = &(m_Cache[metadata.PackageId] = std::move(metadata));
    m_PathLookup[std::move(resolvedPath)] = inserted;
}
}
