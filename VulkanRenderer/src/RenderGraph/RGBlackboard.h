#pragma once

#include "Utils/HashUtils.h"

#include <memory>
#include <unordered_map>

namespace RG
{
    class PassDataTypeIndex
    {
    public:
        template <typename DataType>
        static constexpr u64 Type()
        {
            return Hash::string(GENERATOR_PRETTY_FUNCTION);
        }
        template <typename DataType>
        static constexpr u64 Type(u64 extraHash)
        {
            u64 hash = Hash::string(GENERATOR_PRETTY_FUNCTION);
            Hash::combine(hash, extraHash);
            
            return hash;
        }
    };

    class Blackboard
    {
    public:
        template <typename DataType>
        void Update(const DataType& value);
        template <typename DataType>
        const DataType& Get() const;
        template <typename DataType>
        DataType& Get();

        template <typename DataType>
        const DataType* TryGet() const;
        template <typename DataType>
        DataType* TryGet();

        template <typename DataType>
        void Update(const DataType& value, u64 hash);
        template <typename DataType>
        const DataType& Get(u64 hash) const;
        template <typename DataType>
        DataType& Get(u64 hash);

        template <typename DataType>
        const DataType* TryGet(u64 hash) const;
        template <typename DataType>
        DataType* TryGet(u64 hash);
        
        bool Has(u64 key) const;
    private:
        template <typename DataType>
        void Register(const DataType& value, u64 valueIndex);
    private:
        std::unordered_map<u64, std::shared_ptr<void>> m_Values;
    };

    template <typename DataType>
    void Blackboard::Register(const DataType& value, u64 valueIndex)
    {
        ASSERT(!m_Values.contains(valueIndex), "Value is already registered")

        m_Values.emplace(valueIndex, std::make_shared<DataType>(value));
    }

    template <typename DataType>
    void Blackboard::Update(const DataType& value)
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        if (!Has(valueIndex))
            Register(value, valueIndex);
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
    DataType& Blackboard::Get()
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>());
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
    DataType* Blackboard::TryGet()
    {
        return const_cast<DataType*>(const_cast<const Blackboard&>(*this).TryGet<DataType>());
    }

    template <typename DataType>
    void Blackboard::Update(const DataType& value, u64 hash)
    {
        const u64 valueIndex = PassDataTypeIndex::Type<DataType>(hash);
        if (!Has(valueIndex))
            Register(value, valueIndex);
        else
            m_Values[valueIndex] = std::make_shared<DataType>(value);
    }

    template <typename DataType>
    const DataType& Blackboard::Get(u64 hash) const
    {
        const u64 valueIndex = PassDataTypeIndex::Type<DataType>(hash);
        ASSERT(m_Values.contains(valueIndex), "Value is not registered")

        return *(DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    DataType& Blackboard::Get(u64 hash)
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>(hash));
    }

    template <typename DataType>
    const DataType* Blackboard::TryGet(u64 hash) const
    {
        const u64 valueIndex = PassDataTypeIndex::Type<DataType>(hash);
        if (!m_Values.contains(valueIndex))
            return nullptr;

        return (DataType*)m_Values.at(valueIndex).get();
    }

    template <typename DataType>
    DataType* Blackboard::TryGet(u64 hash)
    {
        return const_cast<DataType*>(const_cast<const Blackboard&>(*this).TryGet<DataType>(hash));
    }

    inline bool Blackboard::Has(u64 key) const
    {
        return m_Values.contains(key);
    }
}

