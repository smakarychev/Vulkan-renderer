#pragma once
#include <memory>
#include <vector>

#include "types.h"
#include "Core/core.h"

namespace RenderGraph
{
    class PassOutputTypeIndex
    {
        inline static u64 s_Counter = 0;
    public:
        template <typename OutputType>
        inline static const u64 Type = s_Counter++;
    };
    
    class Blackboard
    {
    public:
        template <typename OutputType>
        void RegisterOutput(const OutputType& output);
        template <typename OutputType>
        void UpdateOutput(const OutputType& output);
        template <typename OutputType>
        const OutputType& GetOutput() const;
        template <typename OutputType>
        OutputType& GetOutput();
        template <typename OutputType>
        const OutputType* TryGetOutput() const;
        template <typename OutputType>
        OutputType* TryGetOutput();
        template <typename OutputType>
        bool Has() const;

        void Clear();
    private:
        std::vector<std::shared_ptr<void>> m_PassesOutputs;
    };

    inline void Blackboard::Clear()
    {
        m_PassesOutputs.clear();
    }

    template <typename OutputType>
    void Blackboard::RegisterOutput(const OutputType& output)
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
        if (outputIndex >= m_PassesOutputs.size())
            m_PassesOutputs.resize(outputIndex + 1);
        ASSERT(m_PassesOutputs[outputIndex] == nullptr, "Output is already registered")

        m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
    }

    template <typename OutputType>
    void Blackboard::UpdateOutput(const OutputType& output)
    {
        if (!Has<OutputType>())
        {
            RegisterOutput(output);
        }
        else
        {
            u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
            m_PassesOutputs[outputIndex] = std::make_shared<OutputType>(output);
        }
    }

    template <typename OutputType>
    const OutputType& Blackboard::GetOutput() const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
        ASSERT(outputIndex < m_PassesOutputs.size() && m_PassesOutputs[outputIndex] != nullptr,
            "Output is not registered")

        return *(OutputType*)m_PassesOutputs[outputIndex].get();
    }

    template <typename OutputType>
    OutputType& Blackboard::GetOutput()
    {
        return const_cast<OutputType&>(const_cast<const Blackboard&>(*this).GetOutput<OutputType>());
    }

    template <typename OutputType>
    const OutputType* Blackboard::TryGetOutput() const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
        if (outputIndex >= m_PassesOutputs.size() || m_PassesOutputs[outputIndex] == nullptr)
            return nullptr;

        return (OutputType*)m_PassesOutputs[outputIndex].get();
    }

    template <typename OutputType>
    OutputType* Blackboard::TryGetOutput()
    {
        return const_cast<OutputType*>(const_cast<const Blackboard&>(*this).TryGetOutput<OutputType>());
    }

    template <typename OutputType>
    bool Blackboard::Has() const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
        return outputIndex < m_PassesOutputs.size() && m_PassesOutputs[outputIndex] != nullptr;
    }
}

