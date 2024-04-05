#pragma once
#include <memory>
#include <unordered_map>

#include "types.h"
#include "Core/core.h"
#include "utils/hash.h"
#include "utils/StringHasher.h"
#include "utils/utils.h"

namespace RG
{
    class PassDataTypeIndex
    {
    public:
        template <typename DataType>
        static constexpr u64 Type()
        {
            return utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
        template <typename DataType>
        static constexpr u64 Type(utils::StringHasher name)
        {
            return name.GetHash() ^ utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
    };
    
    class Blackboard
    {
    public:
        template <typename DataType>
        void Register(const DataType& value);
        template <typename DataType>
        void Register(utils::StringHasher name, const DataType& value);
        template <typename DataType>
        void Update(const DataType& value);
        template <typename DataType>
        void Update(utils::StringHasher name, const DataType& value);
        template <typename DataType>
        const DataType& Get() const;
        template <typename DataType>
        const DataType& Get(utils::StringHasher name) const;
        template <typename DataType>
        DataType& Get();
        template <typename DataType>
        DataType& Get(utils::StringHasher name);
        template <typename DataType>
        const DataType* TryGet() const;
        template <typename DataType>
        const DataType* TryGet(utils::StringHasher name) const;
        template <typename DataType>
        DataType* TryGet();
        template <typename DataType>
        DataType* TryGet(utils::StringHasher name);
        
        bool Has(u64 key) const;
        
        void Clear();
    private:
        std::unordered_map<u64, std::shared_ptr<void>> m_Values;
    };

    inline void Blackboard::Clear()
    {
        m_Values.clear();
    }

    template <typename DataType>
    void Blackboard::Register(const DataType& value)
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        ASSERT(!m_Values.contains(valueIndex), "Value is already registered")

        m_Values[valueIndex] = std::make_shared<DataType>(value);
    }
    
    template <typename DataType>
    void Blackboard::Register(utils::StringHasher name, const DataType& value)
    {
        u64 valueIndex = PassDataTypeIndex::Type<DataType>(name);
        ASSERT(!m_Values.contains(valueIndex), "Value is already registered")

        m_Values[valueIndex] = std::make_shared<DataType>(value);
    }

    template <typename DataType>
    void Blackboard::Update(const DataType& value)
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        if (!Has(valueIndex))
            Register(value);
        else
            m_Values[valueIndex] = std::make_shared<DataType>(value);
    }

    template <typename DataType>
    void Blackboard::Update(utils::StringHasher name, const DataType& value)
    {
        u64 valueIndex = PassDataTypeIndex::Type<DataType>(name);
        if (!Has(valueIndex))
            Register(name, value);
        else
            m_Values[valueIndex] = std::make_shared<DataType>(value);
    }

    template <typename DataType>
    const DataType& Blackboard::Get() const
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        ASSERT(m_Values.contains(valueIndex), "Value is not registered")

        return *(DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    const DataType& Blackboard::Get(utils::StringHasher name) const
    {
        u64 valueIndex = PassDataTypeIndex::Type<DataType>(name);
        ASSERT(m_Values.contains(valueIndex), "Value is not registered")

        return *(DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    DataType& Blackboard::Get()
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>());
    }

    template <typename DataType>
    DataType& Blackboard::Get(utils::StringHasher name)
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>(name));
    }

    template <typename DataType>
    const DataType* Blackboard::TryGet() const
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        if (!m_Values.contains(valueIndex))
            return nullptr;

        return (DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    const DataType* Blackboard::TryGet(utils::StringHasher name) const
    {
        u64 valueIndex = PassDataTypeIndex::Type<DataType>(name);
        if (!m_Values.contains(valueIndex))
            return nullptr;

        return (DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    DataType* Blackboard::TryGet()
    {
        return const_cast<DataType*>(const_cast<const Blackboard&>(*this).TryGet<DataType>());
    }

    template <typename DataType>
    DataType* Blackboard::TryGet(utils::StringHasher name)
    {
        return const_cast<DataType*>(const_cast<const Blackboard&>(*this).TryGet<DataType>(name));
    }

    inline bool Blackboard::Has(u64 key) const
    {
        return m_Values.contains(key);
    }
}

