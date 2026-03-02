#pragma once

#include "Containers/Guid.h"
#include "Containers/Result.h"

#include <concepts>
#include <functional>
#include <string>
#include <unordered_map>

namespace lux::assetlib::io
{

template<typename T>
concept AssetIoRegistryCreatable = requires (T& type)
{
    {type.GetName()} -> std::same_as<const std::string&>;
    {type.GetGuid()} -> std::same_as<const Guid&>;
};

enum class AssetIoRegistryCreateError : u8
{
    NotFound, Ambiguous
};

template <typename T>
requires AssetIoRegistryCreatable<T>
class AssetIoRegistry
{
public:
    using CreateFn = std::function<std::shared_ptr<T>(void*)>;
public:
    void Register(const std::string& name, const Guid& guid, CreateFn&& createFn);
    Result<std::shared_ptr<T>, AssetIoRegistryCreateError> Create(const std::string& name, void* arguments = nullptr);
    Result<std::shared_ptr<T>, AssetIoRegistryCreateError> Create(const std::string& name, const Guid& guid,
        void* arguments = nullptr);
private:
    struct CreatorInfo
    {
        Guid TypeGuid{};
        CreateFn Creator{};
    };
    std::unordered_map<std::string, std::vector<CreatorInfo>> m_Creators;
};

template <typename T>
requires AssetIoRegistryCreatable<T>
void AssetIoRegistry<T>::Register(const std::string& name, const Guid& guid, CreateFn&& createFn)
{
    if (m_Creators.contains(name))
    {
        for (auto& creator : m_Creators.at(name))
            if (creator.TypeGuid == guid)
                return;

        m_Creators.at(name).push_back(CreatorInfo{.TypeGuid = guid, .Creator = std::move(createFn)});
        return;
    }

    std::vector<CreatorInfo> creators = {CreatorInfo{.TypeGuid = guid, .Creator = std::move(createFn)}};
    m_Creators.emplace(name, std::move(creators));
}

template <typename T>
requires AssetIoRegistryCreatable<T>
Result<std::shared_ptr<T>, AssetIoRegistryCreateError> AssetIoRegistry<T>::Create(const std::string& name,
    void* arguments)
{
    auto it = m_Creators.find(name);
    if (it == m_Creators.end())
        return std::unexpected(AssetIoRegistryCreateError::NotFound);
    
    if (it->second.size() > 1)
        return std::unexpected(AssetIoRegistryCreateError::Ambiguous);

    return it->second.front().Creator(arguments);
}

template <typename T>
requires AssetIoRegistryCreatable<T>
Result<std::shared_ptr<T>, AssetIoRegistryCreateError> AssetIoRegistry<T>::Create(const std::string& name,
    const Guid& guid, void* arguments)
{
    auto it = m_Creators.find(name);
    if (it == m_Creators.end())
        return std::unexpected(AssetIoRegistryCreateError::NotFound);

    for (auto& creatorInfo : it->second)
    {
        if (creatorInfo.TypeGuid != guid)
            continue;

        return creatorInfo.Creator(arguments);
    }

    return std::unexpected(AssetIoRegistryCreateError::NotFound);
}
}