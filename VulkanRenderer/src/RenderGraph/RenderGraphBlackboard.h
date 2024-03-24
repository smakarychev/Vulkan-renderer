#pragma once
#include <memory>
#include <unordered_map>

#include "types.h"
#include "Core/core.h"
#include "utils/hash.h"
#include "utils/StringHasher.h"
#include "utils/utils.h"

namespace RenderGraph
{
    class PassOutputTypeIndex
    {
    public:
        template <typename OutputType>
        static constexpr u64 Type()
        {
            return utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
        template <typename OutputType>
        static constexpr u64 Type(utils::StringHasher name)
        {
            return name.GetHash() ^ utils::hashString(GENERATOR_PRETTY_FUNCTION);
        }
    };
    
    class Blackboard
    {
    public:
        template <typename OutputType>
        void RegisterOutput(const OutputType& output);
        template <typename OutputType>
        void RegisterOutput(utils::StringHasher name, const OutputType& output);
        template <typename OutputType>
        void UpdateOutput(const OutputType& output);
        template <typename OutputType>
        void UpdateOutput(utils::StringHasher name, const OutputType& output);
        template <typename OutputType>
        const OutputType& GetOutput() const;
        template <typename OutputType>
        const OutputType& GetOutput(utils::StringHasher name) const;
        template <typename OutputType>
        OutputType& GetOutput();
        template <typename OutputType>
        OutputType& GetOutput(utils::StringHasher name);
        template <typename OutputType>
        const OutputType* TryGetOutput() const;
        template <typename OutputType>
        const OutputType* TryGetOutput(utils::StringHasher name) const;
        template <typename OutputType>
        OutputType* TryGetOutput();
        template <typename OutputType>
        OutputType* TryGetOutput(utils::StringHasher name);
        
        bool Has(u64 key) const;
        
        void Clear();
    private:
        std::unordered_map<u64, std::shared_ptr<void>> m_PassesOutputs;
    };

    inline void Blackboard::Clear()
    {
        m_PassesOutputs.clear();
    }

    template <typename OutputType>
    void Blackboard::RegisterOutput(const OutputType& output)
    {
        constexpr u64 outputIndex = PassOutputTypeIndex::Type<OutputType>();
        ASSERT(!m_PassesOutputs.contains(outputIndex), "Output is already registered")

        m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
    }
    
    template <typename OutputType>
    void Blackboard::RegisterOutput(utils::StringHasher name, const OutputType& output)
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>(name);
        ASSERT(!m_PassesOutputs.contains(outputIndex), "Output is already registered")

        m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
    }

    template <typename OutputType>
    void Blackboard::UpdateOutput(const OutputType& output)
    {
        constexpr u64 outputIndex = PassOutputTypeIndex::Type<OutputType>();
        if (!Has(outputIndex))
            RegisterOutput(output);
        else
            m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
    }

    template <typename OutputType>
    void Blackboard::UpdateOutput(utils::StringHasher name, const OutputType& output)
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>(name);
        if (!Has(outputIndex))
            RegisterOutput(name, output);
        else
            m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
    }

    template <typename OutputType>
    const OutputType& Blackboard::GetOutput() const
    {
        constexpr u64 outputIndex = PassOutputTypeIndex::Type<OutputType>();
        ASSERT(m_PassesOutputs.contains(outputIndex), "Output is not registered")

        return *(OutputType*)m_PassesOutputs.at(outputIndex).get();
    }

    template <typename OutputType>
    const OutputType& Blackboard::GetOutput(utils::StringHasher name) const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>(name);
        ASSERT(m_PassesOutputs.contains(outputIndex), "Output is not registered")

        return *(OutputType*)m_PassesOutputs.at(outputIndex).get();
    }

    template <typename OutputType>
    OutputType& Blackboard::GetOutput()
    {
        return const_cast<OutputType&>(const_cast<const Blackboard&>(*this).GetOutput<OutputType>());
    }

    template <typename OutputType>
    OutputType& Blackboard::GetOutput(utils::StringHasher name)
    {
        return const_cast<OutputType&>(const_cast<const Blackboard&>(*this).GetOutput<OutputType>(name));
    }

    template <typename OutputType>
    const OutputType* Blackboard::TryGetOutput() const
    {
        constexpr u64 outputIndex = PassOutputTypeIndex::Type<OutputType>();
        if (!m_PassesOutputs.contains(outputIndex))
            return nullptr;

        return (OutputType*)m_PassesOutputs.at(outputIndex).get();
    }

    template <typename OutputType>
    const OutputType* Blackboard::TryGetOutput(utils::StringHasher name) const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>(name);
        if (!m_PassesOutputs.contains(outputIndex))
            return nullptr;

        return (OutputType*)m_PassesOutputs.at(outputIndex).get();
    }

    template <typename OutputType>
    OutputType* Blackboard::TryGetOutput()
    {
        return const_cast<OutputType*>(const_cast<const Blackboard&>(*this).TryGetOutput<OutputType>());
    }

    template <typename OutputType>
    OutputType* Blackboard::TryGetOutput(utils::StringHasher name)
    {
        return const_cast<OutputType*>(const_cast<const Blackboard&>(*this).TryGetOutput<OutputType>(name));
    }

    inline bool Blackboard::Has(u64 key) const
    {
        return m_PassesOutputs.contains(key);
    }
}

