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
        const OutputType& GetOutput() const;        
    private:
        std::vector<std::shared_ptr<void>> m_PassesOutputs;
    };

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
    const OutputType& Blackboard::GetOutput() const
    {
        u64 outputIndex = PassOutputTypeIndex::Type<OutputType>;
        ASSERT(outputIndex < m_PassesOutputs.size() && m_PassesOutputs[outputIndex] != nullptr,
            "Output is not registered")

        return *(OutputType*)m_PassesOutputs[outputIndex].get();
    }
}

