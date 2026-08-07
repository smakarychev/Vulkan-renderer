#pragma once

#include <CoreLib/types.h>

namespace lux
{
// todo: convert all paths to this
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
    
    AssetPath(const std::filesystem::path& path) : m_Path(weakly_canonical(path).generic_string()) {}
    
    operator const std::filesystem::path&() const { return m_Path; }
    
    std::string ToString() const { return m_Path.string(); }
    
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