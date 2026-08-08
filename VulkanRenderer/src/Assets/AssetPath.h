#pragma once

#include <CoreLib/types.h>

namespace lux
{
class AssetPath
{
    friend struct std::hash<AssetPath>;
public:
    AssetPath() = default;
    ~AssetPath() = default;
    AssetPath(const AssetPath&) = default;
    AssetPath& operator=(const AssetPath&) = default;
    AssetPath(AssetPath&&) noexcept = default;
    AssetPath& operator=(AssetPath&&) noexcept = default;
    
    AssetPath(std::string_view filename);
    AssetPath(const std::filesystem::path& rawAssetPath);

    const std::filesystem::path& RawPath() const { return m_Path; }
    std::string RawExtension() const { return m_Path.extension().string(); }

    std::string ToString() const;

    auto operator<=>(const AssetPath&) const = default;
private:
    std::filesystem::path m_Path{};
};
}

namespace std
{
template <>
struct hash<lux::AssetPath>
{
    usize operator()(const lux::AssetPath& path) const noexcept
    {
        return std::hash<std::filesystem::path>{}(path.m_Path);
    }
};
}