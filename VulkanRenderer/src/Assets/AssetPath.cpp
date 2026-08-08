#include "rendererpch.h"

#include "AssetPath.h"
#include "AssetSystem.h"

namespace lux
{
AssetPath::AssetPath(std::string_view filename)
    : AssetPath(AssetSystem::Get().ResolveAssetPath(filename))
{
}

AssetPath::AssetPath(const std::filesystem::path& rawAssetPath)
    : m_Path(rawAssetPath)
{
}

std::string AssetPath::ToString() const
{
    return m_Path.string();
}
}
