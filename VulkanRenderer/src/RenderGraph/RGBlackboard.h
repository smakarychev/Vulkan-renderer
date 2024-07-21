#pragma once
#include <memory>
#include <unordered_map>

#include "RenderPass.h"
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
            return Utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
        template <typename DataType>
        static constexpr u64 Type(Utils::StringHasher name)
        {
            return name.GetHash() ^ Utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
    };

    // todo: remove all `Utils::StringHasher` variants 
    class Blackboard
    {
    public:
        template <typename DataType>
        void Update(const DataType& value);
        template <typename DataType>
        void Update(Utils::StringHasher name, const DataType& value);
        template <typename DataType>
        const DataType& Get() const;
        template <typename DataType>
        const DataType& Get(Utils::StringHasher name) const;
        template <typename DataType>
        DataType& Get();
        template <typename DataType>
        DataType& Get(Utils::StringHasher name);

        template <typename DataType>
        DataType& Get(const Pass& pass);
        
        template <typename DataType>
        const DataType* TryGet() const;
        template <typename DataType>
        const DataType* TryGet(Utils::StringHasher name) const;
        template <typename DataType>
        DataType* TryGet();
        template <typename DataType>
        DataType* TryGet(Utils::StringHasher name);
        
        bool Has(u64 key) const;
    private:
        template <typename DataType>
        void Register(const DataType& value);
        template <typename DataType>
        void Register(Utils::StringHasher name, const DataType& value);
    private:
        std::unordered_map<u64, std::shared_ptr<void>> m_Values;
    };

    template <typename DataType>
    void Blackboard::Register(const DataType& value)
    {
        constexpr u64 valueIndex = PassDataTypeIndex::Type<DataType>();
        ASSERT(!m_Values.contains(valueIndex), "Value is already registered")

        m_Values[valueIndex] = std::make_shared<DataType>(value);
    }
    
    template <typename DataType>
    void Blackboard::Register(Utils::StringHasher name, const DataType& value)
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
    void Blackboard::Update(Utils::StringHasher name, const DataType& value)
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
    const DataType& Blackboard::Get(Utils::StringHasher name) const
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
    DataType& Blackboard::Get(Utils::StringHasher name)
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>(name));
    }

    template <typename DataType>
    DataType& Blackboard::Get(const Pass& pass)
    {
        return const_cast<DataType&>(const_cast<const Blackboard&>(*this).Get<DataType>(pass.GetNameHash()));
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
    const DataType* Blackboard::TryGet(Utils::StringHasher name) const
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
    DataType* Blackboard::TryGet(Utils::StringHasher name)
    {
        return const_cast<DataType*>(const_cast<const Blackboard&>(*this).TryGet<DataType>(name));
    }

    inline bool Blackboard::Has(u64 key) const
    {
        return m_Values.contains(key);
    }
}

