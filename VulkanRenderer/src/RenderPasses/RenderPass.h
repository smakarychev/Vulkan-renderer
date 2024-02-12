#pragma once

class RenderPassResource
{
    
};

class RenderPassResourceAccess
{
    
};

class RenderPassResources
{
    
};



class RenderPass
{
    class ExecutionCallbackBase
    {
    public:
        virtual ~ExecutionCallbackBase() = default;
        virtual void Execute(FrameContext* frameContext, const RenderPassResources& passResources) = 0;
    };
    
    template <typename PassData, typename ExecutionLambda>
    class ExecutionCallback final : public ExecutionCallbackBase
    {
    public:
        ExecutionCallback(const PassData& passData, ExecutionLambda&& executionLambda)
            : m_ExecutionLambda(std::forward<ExecutionLambda>(executionLambda)), m_PassData(passData)
        {}
        void Execute(FrameContext* frameContext, const RenderPassResources& passResources) override
        {
            m_ExecutionLambda(m_PassData, frameContext, passResources);
        }
    private:
        ExecutionLambda m_ExecutionLambda;
        PassData m_PassData;
    };
public:
    
private:
    std::unique_ptr<ExecutionCallbackBase> m_ExecutionCallback;
    std::vector<RenderPassResource> m_Creates;
    std::vector<RenderPassResourceAccess> m_Reads;
    std::vector<RenderPassResourceAccess> m_Writes;

    std::string m_Name;
};