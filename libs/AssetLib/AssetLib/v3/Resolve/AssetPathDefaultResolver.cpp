#include "AssetPathDefaultResolver.h"

namespace lux::assetlib
{
std::optional<AssetResolvedPath> AssetPathDefaultResolver::Resolve(const std::string& path) const
{
    {
        const std::scoped_lock lock(m_Mutex);
        auto it = m_ResolvedPathCache.find(path);
        if (it != m_ResolvedPathCache.end())
            return it->second;
    }

    auto preprocessed = Preprocess(path);
    if (!preprocessed)
        return std::nullopt;

    auto decomposed = Decompose(*preprocessed);
    if (!decomposed)
        return std::nullopt;

    auto canonicalPackagePath = GetCanonicalPackagePath(decomposed->Package);
    if (!canonicalPackagePath)
        return std::nullopt;

    {
        const std::scoped_lock lock(m_Mutex);
        AssetResolvedPath resolvedPath(
            std::move(*canonicalPackagePath), 
            std::move(decomposed->AssetName), 
            std::move(decomposed->Variant));
        
        m_InverseLookupCache[resolvedPath].push_back(path);
        
        return m_ResolvedPathCache[path] = resolvedPath;
    }
}

void AssetPathDefaultResolver::InvalidateCache()
{
    const std::scoped_lock lock(m_Mutex);
    m_ResolvedPathCache.clear();
    m_InverseLookupCache.clear();
}

void AssetPathDefaultResolver::InvalidateCache(const AssetResolvedPath& path)
{
    const std::scoped_lock lock(m_Mutex);
    auto it = m_InverseLookupCache.find(path);
    if (it == m_InverseLookupCache.end())
        return;
    
    for (auto& rawPath : it->second)
        m_ResolvedPathCache.erase(rawPath);
    m_InverseLookupCache.erase(it);
}

void AssetPathDefaultResolver::AddProtocolResolution(const std::string& protocol, const std::filesystem::path& path)
{
    const std::scoped_lock lock(m_Mutex);
    m_ProtocolMap[protocol] = std::filesystem::weakly_canonical(path).generic_string();
}

void AssetPathDefaultResolver::AddSearchPath(const std::filesystem::path& path)
{
    auto canonical = std::filesystem::weakly_canonical(path).generic_string();
    const std::scoped_lock lock(m_Mutex);
    if (std::ranges::find(m_SearchPaths, canonical) != m_SearchPaths.end())
        return;
    m_SearchPaths.emplace_back(canonical);
}

std::optional<std::string> AssetPathDefaultResolver::GetCanonicalPackagePath(const std::string& path) const
{
    auto packagePath = FindPackagePath(path);
    if (!packagePath)
        return std::nullopt;
    
    PostprocessInplace(*packagePath);
    
    return std::move(*packagePath);
}

std::optional<std::string> AssetPathDefaultResolver::FindPackagePath(const std::string& path) const
{
    const u64 protocolPosition = path.find(':');
    const bool isAbsolute = std::filesystem::path(path).is_absolute();
    const bool isProtocol = !isAbsolute && protocolPosition != std::string::npos;
    const bool isRelative = !isProtocol && !isAbsolute;
    
    const std::scoped_lock lock(m_Mutex);
    if (isRelative)
    {
        for (auto& searchPath : m_SearchPaths)
        {
            auto packagePath = searchPath / path;
            if (std::filesystem::exists(packagePath))
                return packagePath.string();
        }
        
        return std::nullopt;
    }
    if (isAbsolute)
    {
        if (!std::filesystem::exists(path))
            return std::nullopt;
        
        for (auto& searchPath : m_SearchPaths)
        {
            auto relative = std::filesystem::relative(path, searchPath);
            if (!relative.empty())
                return path;
        }
        
        return std::nullopt;
    }
    
    auto it = m_ProtocolMap.find(path.substr(0, protocolPosition));
    if (it == m_ProtocolMap.end())
        return std::nullopt;
    
    const u64 protocolEnd = path.find_first_not_of('/', protocolPosition + 1);
    auto packagePath = it->second / path.substr(protocolEnd);
    
    return std::filesystem::exists(packagePath) ? std::optional(packagePath.string()) : std::nullopt;
}

std::optional<AssetPathDefaultResolver::Decomposed> AssetPathDefaultResolver::Decompose(const std::string& path) const
{
    std::string_view pathView = path;
    
    const u64 dot = pathView.find('.');
    if (dot == pathView.npos || dot == pathView.size() || dot == 0)
        return std::nullopt;
    
    auto package = std::string(pathView.substr(0, dot - 0)) + ".meta";
    
    const u64 variantPosition = pathView.find('{');
    if (variantPosition == pathView.npos)
        return Decomposed{
            .Package = std::move(package),
            .AssetName = std::string(pathView.substr(dot + 1)),
        };
    
    const u64 variantEndPosition = pathView.find('}');
    if (variantEndPosition == pathView.npos)
        return std::nullopt;
    
    return Decomposed{
        .Package = std::move(package),
        .AssetName = std::string(pathView.substr(dot + 1, variantPosition - dot - 1)),
        .Variant = std::string(pathView.substr(variantPosition + 1, variantEndPosition - variantPosition - 1)),
    };
}

std::optional<std::string> AssetPathDefaultResolver::Preprocess(const std::string& path) const
{
    /* path cannot be absolute */
    if (path.starts_with('/'))
        return std::nullopt;
    
    /* it is not valid to have '..' in the path */
    if (path.find("..") != path.npos)
        return std::nullopt;

    std::string preprocessed;
    preprocessed.reserve(path.size());
    
    u64 offset = 0;
    if (path.starts_with("./"sv))
        offset += "./"sv.size();

    while (offset < path.size())
    {
        const char c = path[offset];
        if (c == '.')
        {
            if (offset + 1 < path.size() && path[offset + 1] == '/')
                offset += 1;
            else
                preprocessed += '.';
        }
        else
        {
            preprocessed += c;
        }
        offset += 1;
    }
    
    return preprocessed;
}

void AssetPathDefaultResolver::PostprocessInplace(std::string& path) const
{
    for (auto& c : path)
    {
        if (c == '\\')
            c = '/';
        else
            c = (char)std::tolower(c);
    }
}
}
