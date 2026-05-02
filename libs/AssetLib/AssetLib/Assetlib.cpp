#include "Assetlib.h"

#include "Reflection/AssetlibReflectionUtility.inl"

namespace lux::assetlib
{
bool isMetadataPath(const std::filesystem::path& path)
{
    return path.extension() == ASSETLIB_METADATA_EXTENSION;
}

std::filesystem::path getMetadataPath(const std::filesystem::path& path)
{
    if (isMetadataPath(path))
        return path;
    
    std::filesystem::path metaPath = path;
    metaPath += ASSETLIB_METADATA_EXTENSION;

    return metaPath;
}

std::string getMetadataRawExtension(const std::filesystem::path& path)
{
    if (!isMetadataPath(path))
        return {};
    
    return path.stem().extension().string();
}
}


