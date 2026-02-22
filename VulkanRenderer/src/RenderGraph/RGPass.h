#pragma once

#include "RGAccess.h"
#include "Assets/Shaders/ShaderAssetManager.h"
#include "Rendering/Synchronization.h"
#include "String/StringId.h"

struct FrameContext;

namespace RG
{
class Graph;

enum class PassFlags
{
    None = 0,
    Disabled = BIT(1),
    Cullable = BIT(2),
    Rasterization = BIT(3)
};

CREATE_ENUM_FLAGS_OPERATORS(PassFlags)

template <typename Fn, typename PassData>
concept PassSetupFn = requires(Fn setup, Graph& graph, PassData& passData)
{
    { setup(graph, passData) } -> std::same_as<void>;
};
template <typename Fn, typename PassData>
concept PassExecuteFn = requires(Fn execute, const PassData& passData, FrameContext& ctx, const Graph& graph)
{
    { execute(passData, ctx, graph) } -> std::same_as<void>;
};
template <typename Setup, typename Execute, typename PassData>
concept PassCallbacksFn = PassSetupFn<Setup, PassData> && PassExecuteFn<Execute, PassData>;

template <typename PassData, typename BindGroupType>
struct PassDataWithBind : PassData
{
    BindGroupType BindGroup{};
};

class Pass
{
    friend class Graph;

    class ExecutionCallbackBase
    {
    public:
        virtual ~ExecutionCallbackBase() = default;
        virtual void Execute(FrameContext& frameContext, const Graph& graph) = 0;
        virtual void* GetPassData() = 0;
    };

    template <typename PassData, typename ExecutionLambda>
    class ExecutionCallback final : public ExecutionCallbackBase
    {
        friend class Graph;

    public:
        ExecutionCallback(const PassData& passData, ExecutionLambda&& executionLambda)
            : m_ExecutionLambda(std::forward<ExecutionLambda>(executionLambda)), m_PassData(passData)
        {
            static_assert((u32)sizeof(executionLambda) <= 1 * 1024);
        }

        void Execute(FrameContext& frameContext, const Graph& graph) override
        {
            m_ExecutionLambda(m_PassData, frameContext, graph);
        }

        void* GetPassData() override
        {
            return &m_PassData;
        }

    private:
        ExecutionLambda m_ExecutionLambda;
        PassData m_PassData;
    };

public:
    Pass(StringId name)
        : m_Name(name)
    {
    }

    void Execute(FrameContext& frameContext, const Graph& graph) const
    {
        m_ExecutionCallback->Execute(frameContext, graph);
    }

    StringId Name() const { return m_Name; }

private:
    std::unique_ptr<ExecutionCallbackBase> m_ExecutionCallback;
    std::vector<DependencyInfo> m_BarriersToWait;

    struct SplitDependency
    {
        DependencyInfo Dependency{};
        SplitBarrier Barrier{};
    };

    std::vector<SplitDependency> m_SplitBarriersToSignal;
    std::vector<SplitDependency> m_SplitBarriersToWait;

    struct ImageLayoutInfo
    {
        Resource Image{};
        ImageLayout Layout{ImageLayout::Undefined};
    };

    std::vector<ImageLayoutInfo> m_ImageLayouts;

    std::vector<RenderTargetAccess> m_RenderTargets;
    DepthStencilTargetAccess m_DepthStencilTargetAccess{};

    PassFlags m_Flags{PassFlags::Cullable};
    lux::Shader m_Shader{};

    StringId m_Name;
};
}
