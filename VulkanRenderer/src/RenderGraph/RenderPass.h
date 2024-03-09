#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "RenderGraphResource.h"
#include "Rendering/Buffer.h"
#include "Rendering/Synchronization.h"

class DependencyInfo;
struct FrameContext;

namespace RenderGraph
{
    class Resources;
    
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
            {}
            void Execute(FrameContext& frameContext, const Resources& passResources) override
            {
                m_ExecutionLambda(m_PassData, frameContext, passResources);
            }
        private:
            ExecutionLambda m_ExecutionLambda;
            PassData m_PassData;
        };
    public:
        Pass(const std::string& name)
            : m_Name(name) {}

        // todo: temp remove me?
        void Execute(FrameContext& frameContext, const Resources& passResources) const
        {
            m_ExecutionCallback->Execute(frameContext, passResources);
        }
    
    private:
        std::unique_ptr<ExecutionCallbackBase> m_ExecutionCallback;
        std::vector<ResourceAccess> m_Accesses;
        std::vector<RenderTargetAccess> m_RenderTargetAttachmentAccess;
        std::optional<DepthStencilAccess> m_DepthStencilAccess;

        std::vector<DependencyInfo> m_LayoutTransitions;
        std::vector<DependencyInfo> m_Barriers;
        bool m_IsRasterizationPass{false};
        // passes that write to external resources, cannot be culled
        bool m_CanBeCulled{true};
        
        // mainly for mermaid dump
        struct PassTextureTransitionInfo
        {
            Resource Texture;
            ImageLayout OldLayout;
            ImageLayout NewLayout;
        };
        std::vector<PassTextureTransitionInfo> m_LayoutTransitionInfos;
        struct BarrierDependencyInfo
        {
            Resource Resource;
            std::optional<ExecutionDependencyInfo> ExecutionDependency;
            std::optional<MemoryDependencyInfo> MemoryDependency;
        };
        std::vector<BarrierDependencyInfo> m_BarrierDependencyInfos;
    
        std::string m_Name;
    };    
}
