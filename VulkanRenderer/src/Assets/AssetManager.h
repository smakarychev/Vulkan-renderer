#pragma once
#include "AssetHandle.h"

#include <AssetLib/Io/AssetIo.h>
#include <CoreLib/Containers/Guid.h>

#include <mutex>

namespace lux
{
class AssetPath;

namespace import
{
struct Context;
}

class AssetIdResolver;
}

namespace lux
{
class AssetSystem;

struct AssetLoadParameters
{
    auto operator<=>(const AssetLoadParameters&) const = default;
};

class AssetManager
{
public:
    using IoError = assetlib::io::IoError;
public:
    AssetManager();
    virtual ~AssetManager() = default;
    virtual const std::string& GetName() const = 0;
    virtual const Guid& GetGuid() const = 0;

    /* is called by `AssetSystem` when all managers are registered and before the directory scan is performed */
    virtual void OnAssetSystemInit() {}
    virtual bool AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath) = 0;
    virtual bool Imports(std::string_view extension) = 0;
    virtual void OnFileModified(const AssetPath& path) = 0;
    
    virtual AssetHandleBase Load(const AssetLoadParameters& parameters) = 0;
    virtual void Reload(AssetHandleBase handle) = 0;
    virtual void Unload(AssetHandleBase handle) = 0;
protected:
    std::shared_ptr<import::Context> m_Ctx{nullptr};
};

template <typename Resource>
struct ResourceAssetLoadParameters : AssetLoadParameters
{
    auto operator<=>(const ResourceAssetLoadParameters&) const = default;
};
template <typename Resource>
struct ResourceAssetLoadParametersHasher
{
    using Parameters = ResourceAssetLoadParameters<Resource>;
    usize operator()(const Parameters& parameters) const noexcept
    {
        return std::hash<Parameters>{}(parameters);
    }
};
template <typename Resource>
struct ResourceAssetLoadParametersSourceHasher
{
    using Source = ResourceAssetLoadParameters<Resource>::SourceType;
    usize operator()(const Source& source) const noexcept
    {
        return std::hash<Source>{}(source);
    }
};

struct ResourceAssetManagerTraits
{
    using Mutex = std::mutex;
    using Lock = std::lock_guard<std::mutex>;
};

struct ResourceAssetManagerTraitsNoLock
{
    struct Mutex
    {
        void lock() {}
        void unlock() {}
    };
    struct Lock
    {
        Lock(Mutex&) {}
    };
};

struct ResourceAssetTraits
{  
    template <typename Resource>    
    using GetType = const Resource*;  
};    
    
struct ResourceAssetTraitsGetOptional : ResourceAssetTraits
{  
    template <typename Resource>    
    using GetType = std::optional<Resource>;  
};
    
struct ResourceAssetTraitsGetValue : ResourceAssetTraits
{  
    template <typename Resource>    
    using GetType = Resource;  
};

template <typename Resource>
concept ResourceHasSource = requires { typename ResourceAssetLoadParameters<Resource>::SourceType; };

struct ResourceAssetManagerCacheEmptySourceIndex {};

template <typename Resource, bool = ResourceHasSource<Resource>>
class ResourceAssetManagerCacheSourceIndexResolver
{
public:
    using SourceKey = ResourceAssetManagerCacheEmptySourceIndex;
    using Index = ResourceAssetManagerCacheEmptySourceIndex;
};

template <typename Resource>
class ResourceAssetManagerCacheSourceIndexResolver<Resource, true>
{
public:
    using Parameters = ResourceAssetLoadParameters<Resource>;
    using SourceKey = Parameters::SourceType;
    using SourceKeyHasher = ResourceAssetLoadParametersSourceHasher<Resource>;
public:
    void Add(const SourceKey& key, AssetHandle<Resource> handle);
    void Remove(const SourceKey& key, AssetHandle<Resource> handle);
    const std::vector<AssetHandle<Resource>>* Find(const SourceKey& key);
private:
    std::unordered_map<SourceKey, std::vector<AssetHandle<Resource>>, SourceKeyHasher> m_Index;
};

template <typename Resource>
using ResourceAssetManagerCacheSourceIndex = ResourceAssetManagerCacheSourceIndexResolver<Resource>;

template <typename Resource>
class ResourceAssetManagerCache
{
public:
    using Key = ResourceAssetLoadParameters<Resource>;
    using KeyHasher = ResourceAssetLoadParametersHasher<Resource>;
    using SourceKey = ResourceAssetManagerCacheSourceIndex<Resource>::SourceKey;
public:
    void Add(const Key& key, AssetHandle<Resource> handle);
    void Remove(const Key& key);
    void Remove(AssetHandle<Resource> handle);
    bool Has(const Key& key) const;
    AssetHandle<Resource> Find(const Key& key) const;
    const std::vector<AssetHandle<Resource>>* FindBySource(const SourceKey& source) requires ResourceHasSource<Resource>;

    const Key* FindKeyForHandle(AssetHandle<Resource> handle) const;

    template <typename Fn>
    void IterateCache(Fn&& fn)
    {
        for (auto&& [key, handle] : m_Cache)
            fn(key, handle);
    }
private:
    std::unordered_map<Key, AssetHandle<Resource>, KeyHasher> m_Cache;
    // todo: maybe it is not the best way
    std::unordered_map<AssetHandle<Resource>, Key> m_KeyLookUp;
    [[no_unique_address]] ResourceAssetManagerCacheSourceIndex<Resource> m_AssetsBySource;
};

template <typename Resource, typename ResourceAssetTraits, typename ManagerTraits = ResourceAssetManagerTraits>
class ResourceAssetManager : public AssetManager
{
    static_assert(sizeof(AssetHandle<Resource>) == sizeof(AssetHandleBase));
public:
    using GetType = ResourceAssetTraits::template GetType<Resource>;  
    using Lock = ManagerTraits::Lock;
    using Mutex = ManagerTraits::Mutex;
    
    GetType Get(AssetHandle<Resource> handle) const
    {
        Lock lock(m_ResourceAccessMutex);
        
        return GetAsset(handle);
    }

    AssetHandle<Resource> LoadResource(const ResourceAssetLoadParameters<Resource>& parameters)
    {
        return (AssetHandle<Resource>)Load((const AssetLoadParameters&)parameters);
    }

    void UnloadResource(AssetHandle<Resource> handle)
    {
        return Unload(handle);
    }
private:
    AssetHandleBase Load(const AssetLoadParameters& parameters) final
    {
        Lock lock(m_ResourceAccessMutex);

        auto cached = (AssetHandleBase)m_Cache.Find(
            (const ResourceAssetLoadParameters<Resource>&)parameters);
        if (cached.IsValid())
            return cached;
        
        cached = LoadAsset((const ResourceAssetLoadParameters<Resource>&)parameters);
        m_Cache.Add((const ResourceAssetLoadParameters<Resource>&)parameters, (AssetHandle<Resource>)cached);
        
        return cached;
    }
    
    void Reload(AssetHandleBase handle) final
    {
        Lock lock(m_ResourceAccessMutex);
        
        ReloadAsset((AssetHandle<Resource>)handle);
    }

    void Unload(AssetHandleBase handle) final
    {
        Lock lock(m_ResourceAccessMutex);
        
        UnloadAsset((AssetHandle<Resource>)handle);
        m_Cache.Remove(handle);
    }
protected:
    virtual AssetHandle<Resource> LoadAsset(const ResourceAssetLoadParameters<Resource>& parameters) = 0;
    virtual void ReloadAsset(AssetHandle<Resource> handle) = 0;
    virtual void UnloadAsset(AssetHandle<Resource> handle) = 0;
    virtual GetType GetAsset(AssetHandle<Resource> handle) const = 0;

    template <typename Fn>
    requires ResourceHasSource<Resource>
    void IterateBySourceUnsafe(const ResourceAssetManagerCacheSourceIndexResolver<Resource>::SourceKey& source, Fn&& fn)
    {
        auto* assets = m_Cache.FindBySource(source);
        if (!assets || assets->empty())
            return;
        for (AssetHandle<Resource> handle : *assets)
            fn(handle);
    }

    const ResourceAssetLoadParameters<Resource>* FindLoadParametersUnsafe(AssetHandle<Resource> handle)
    {
        return m_Cache.FindKeyForHandle(handle);
    }
    
    AssetHandle<Resource> FindHandleByLoadParametersUnsafe(const ResourceAssetLoadParameters<Resource>& parameters)
    {
        return m_Cache.Find(parameters);
    }
    
    template <typename Fn>
    void IterateCacheUnsafe(Fn&& fn)
    {
        m_Cache.IterateCache(std::forward<Fn>(fn));
    }
protected:
    mutable Mutex m_ResourceAccessMutex;
private:
    ResourceAssetManagerCache<Resource> m_Cache{};
};

template <typename Resource>
void ResourceAssetManagerCacheSourceIndexResolver<Resource, true>::Add(const SourceKey& key,
    AssetHandle<Resource> handle)
{
    m_Index[key].push_back(handle);
}

template <typename Resource>
void ResourceAssetManagerCacheSourceIndexResolver<Resource, true>::Remove(const SourceKey& key, 
    AssetHandle<Resource> handle)
{
    auto& handles = m_Index[key];
    auto it = std::ranges::find(handles, handle);
    ASSERT(it != handles.end(), "Trying to remove item that is not present in the cache")
    if (it == handles.end())
        return;
    
    *it = std::move(handles.back());
    handles.pop_back();
    
    if (handles.empty())
        m_Index.erase(key);
}

template <typename Resource>
const std::vector<AssetHandle<Resource>>* ResourceAssetManagerCacheSourceIndexResolver<Resource, true>::Find(
    const SourceKey& key)
{
    const auto it = m_Index.find(key);
    if (it == m_Index.end())
        return nullptr;

    return &it->second;
}

template <typename Resource>
void ResourceAssetManagerCache<Resource>::Add(const Key& key, AssetHandle<Resource> handle)
{
    ASSERT(!m_Cache.contains(key), "Duplicate key found")
    if (m_Cache.contains(key))
        return;
    
    m_Cache.emplace(key, handle);
    m_KeyLookUp.emplace(handle, key);
    if constexpr (ResourceHasSource<Resource>)
        m_AssetsBySource.Add(key.ToSourceType(), handle);
}

template <typename Resource>
void ResourceAssetManagerCache<Resource>::Remove(const Key& key)
{
    auto it = m_Cache.find(key);
    ASSERT(it != m_Cache.end(), "Trying to remove item that is not present in the cache")
    if (it == m_Cache.end())
        return;
    
    if constexpr (ResourceHasSource<Resource>)
        m_AssetsBySource.Remove(key.ToSourceType(), it->second);
    m_KeyLookUp.erase(it->second);
    m_Cache.erase(it);
}

template <typename Resource>
void ResourceAssetManagerCache<Resource>::Remove(AssetHandle<Resource> handle)
{
    auto it = m_KeyLookUp.find(handle);
    ASSERT(it != m_KeyLookUp.end(), "Trying to remove item that is not present in the cache")
    if (it == m_KeyLookUp.end())
        return;
    
    Remove(it->second);
}

template <typename Resource>
bool ResourceAssetManagerCache<Resource>::Has(const Key& key) const
{
    return m_Cache.contains(key);
}

template <typename Resource>
AssetHandle<Resource> ResourceAssetManagerCache<Resource>::Find(const Key& key) const
{
    const auto it = m_Cache.find(key);
    if (it == m_Cache.end())
        return {};
        
    return it->second;
}

template <typename Resource>
const std::vector<AssetHandle<Resource>>* ResourceAssetManagerCache<Resource>::FindBySource(const SourceKey& source)
    requires ResourceHasSource<Resource>
{
    return m_AssetsBySource.Find(source);
}

template <typename Resource>
const ResourceAssetManagerCache<Resource>::Key* ResourceAssetManagerCache<Resource>::FindKeyForHandle(
    AssetHandle<Resource> handle) const
{
    auto it = m_KeyLookUp.find(handle);
    if (it == m_KeyLookUp.end())
        return nullptr;
    
    return &it->second;
}
}

#define LUX_ASSET_MANAGER(_x, _guid) \
    static const std::string& GetNameStatic() { static const std::string name = #_x; return name; } \
    const std::string& GetName() const override { return GetNameStatic(); } \
    static const Guid& GetGuidStatic() { static constexpr Guid guid = _guid; return guid; } \
    const Guid& GetGuid() const override { return GetGuidStatic(); }