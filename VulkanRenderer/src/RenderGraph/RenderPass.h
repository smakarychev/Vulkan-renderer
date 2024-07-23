#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "RGResource.h"
#include "Rendering/Synchronization.h"
#include "utils/HashedString.h"

class DependencyInfo;
struct FrameContext;

namespace RG
{
    class Resources;

    class PassName
    {
        friend class Graph;
        friend class Pass;
    public:
        PassName(std::string_view name) :
            m_Name(name), m_Hash(Utils::hashString(name)) {}
        
        const std::string& Name() const { return m_Name; }
        u64 Hash() const { return m_Hash; }
    private:
        std::string m_Name{};
        u64 m_Hash{};
    };
    
    class Pass
    {
        friend class Graph;
    
        class ExecutionCallbackBase
        {
        public:
            virtual ~ExecutionCallbackBase() = default;
            virtual void Execute(FrameContext& frameContext, const Resources& passResources) = 0;
        };
    
        template <typename PassData, typename ExecutionLambda>
        class ExecutionCallback final : public ExecutionCallbackBase
        {
        public:
            ExecutionCallback(const PassData& passData, ExecutionLambda&& executionLambda)
                : m_ExecutionLambda(std::forward<ExecutionLambda>(executionLambda)), m_PassData(passData)
            {
                static_assert((u32)sizeof(executionLambda) <= 1 * 1024);
            }
            void Execute(FrameContext& frameContext, const Resources& passResources) override
            {
                m_ExecutionLambda(m_PassData, frameContext, passResources);
            }
        private:
            ExecutionLambda m_ExecutionLambda;
            PassData m_PassData;
        };
    public:
        Pass(std::string_view name)
            : m_Name(name) {}
        Pass(const PassName& name)
            : m_Name(name) {}

        void Execute(FrameContext& frameContext, const Resources& passResources) const
        {
            m_ExecutionCallback->Execute(frameContext, passResources);
        }

        std::string_view GetNameString() const { return m_Name.m_Name; }
        u64 GetNameHash() const { return m_Name.m_Hash; }
        
    private:
        void Reset()
        {
            m_ExecutionCallback.reset();
            m_Accesses.clear();
            m_RenderTargetAttachmentAccess.clear();
            m_DepthStencilAccess.reset();
            m_Barriers.clear();
            m_SplitBarriersToSignal.clear();
            m_SplitBarriersToWait.clear();
            m_IsRasterizationPass = false;
            m_CanBeCulled = false;

            m_BarrierDependencyInfos.clear();
            m_SplitBarrierSignalInfos.clear();
            m_SplitBarrierWaitInfos.clear();
        }
    private:
        std::unique_ptr<ExecutionCallbackBase> m_ExecutionCallback;
        std::vector<ResourceAccess> m_Accesses;
        std::vector<RenderTargetAccess> m_RenderTargetAttachmentAccess;
        std::optional<DepthStencilAccess> m_DepthStencilAccess;

        std::vector<DependencyInfo> m_Barriers;
        struct SplitDependency
        {
            DependencyInfo Dependency;
            SplitBarrier Barrier;
        };
        std::vector<SplitDependency> m_SplitBarriersToSignal;
        std::vector<SplitDependency> m_SplitBarriersToWait;
        
        bool m_IsRasterizationPass{false};
        // passes that write to external resources cannot be culled
        bool m_CanBeCulled{true};
        
        // mainly for mermaid dump
        struct BarrierDependencyInfo
        {
            Resource Resource;
            std::optional<ExecutionDependencyInfo> ExecutionDependency;
            std::optional<MemoryDependencyInfo> MemoryDependency;
            std::optional<LayoutTransitionInfo> LayoutTransition;
        };
        std::vector<BarrierDependencyInfo> m_BarrierDependencyInfos;
        std::vector<BarrierDependencyInfo> m_SplitBarrierSignalInfos;
        std::vector<BarrierDependencyInfo> m_SplitBarrierWaitInfos;
    
        PassName m_Name;
    };    
}
