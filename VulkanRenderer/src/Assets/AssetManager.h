#pragma once
#include "AssetHandle.h"
#include "Containers/Guid.h"
#include "v2/Io/AssetIo.h"

#include <mutex>

namespace lux
{
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
    AssetManager(AssetSystem& system) : m_AssetSystem(&system) {}
    virtual ~AssetManager() = default;
    virtual const std::string& GetName() const = 0;
    virtual const Guid& GetGuid() const = 0;

    virtual bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) = 0;
    virtual bool Bakes(const std::filesystem::path& path) = 0;
    virtual void OnFileModified(const std::filesystem::path& path) = 0;
    
    virtual AssetHandleBase Load(const AssetLoadParameters& parameters) = 0;
    virtual void Unload(AssetHandleBase handle) = 0;
protected:
    AssetSystem* m_AssetSystem{nullptr};
};

template <typename Resource>
struct ResourceAssetLoadParameters : AssetLoadParameters
{
    auto operator<=>(const ResourceAssetLoadParameters&) const = default;
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

template <typename Resource, typename ResourceAssetTraits, typename ManagerTraits = ResourceAssetManagerTraits>
class ResourceAssetManager : public AssetManager
{
    static_assert(sizeof(AssetHandle<Resource>) == sizeof(AssetHandleBase));
public:
    using GetType = ResourceAssetTraits::template GetType<Resource>;  
    using Lock = ManagerTraits::Lock;
    using Mutex = ManagerTraits::Mutex;
    
    ResourceAssetManager(AssetSystem& system) : AssetManager(system) {}
    
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
        
        return LoadAsset((const ResourceAssetLoadParameters<Resource>&)parameters);
    }

    void Unload(AssetHandleBase handle) final
    {
        Lock lock(m_ResourceAccessMutex);
        
        return UnloadAsset((AssetHandle<Resource>)handle);
    }
protected:
    virtual AssetHandle<Resource> LoadAsset(const ResourceAssetLoadParameters<Resource>& parameters) = 0;
    virtual void UnloadAsset(AssetHandle<Resource> handle) = 0;
    virtual GetType GetAsset(AssetHandle<Resource> handle) const = 0;
protected:
    mutable Mutex m_ResourceAccessMutex;
};
}

#define LUX_ASSET_MANAGER(_x, _guid) \
    static const std::string& GetNameStatic() { static const std::string name = #_x; return name; } \
    const std::string& GetName() const override { return GetNameStatic(); } \
    static const Guid& GetGuidStatic() { static constexpr Guid guid = _guid; return guid; } \
    const Guid& GetGuid() const override { return GetGuidStatic(); } \
    _x(AssetSystem& system) : ResourceAssetManager(system) {}