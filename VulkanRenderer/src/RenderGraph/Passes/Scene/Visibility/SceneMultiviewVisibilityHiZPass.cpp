#include "rendererpch.h"

#include "SceneMultiviewVisibilityHiZPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/HiZ/HiZPass.h"

Passes::SceneMultiviewVisibilityHiz::PassData& Passes::SceneMultiviewVisibilityHiz::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData&)
        {
            CPU_PROFILE_FRAME("SceneMultiviewVisibilityHiz.Setup")

            graph.HasSideEffect();
            
            for (u32 i = 0; i < info.MultiviewVisibility->VisibilityCount(); i++)
            {
                auto& view = info.MultiviewVisibility->View({i});
                const bool requireHiZ =
                    info.Depths[i].IsValid() &&
                    enumHasAny(view.ViewInfo.Camera.VisibilityFlags, VisibilityFlags::OcclusionCull) &&
                    !info.Resources->Hiz[i].IsValid();
                if (!requireHiZ)
                    continue;

                const bool isPrimaryView =
                    enumHasAny(view.ViewInfo.Camera.VisibilityFlags, VisibilityFlags::IsPrimaryView);
                auto& hiz = HiZ::addToGraph(name.AddVersion(i), graph, {
                        .Depth = info.Depths[i],
                        .ReductionMode = isPrimaryView ? ::HiZ::ReductionMode::MinMax : ::HiZ::ReductionMode::Min,
                        .CalculateMinMaxDepthBuffer = isPrimaryView
                    });
                
                info.Resources->Hiz[i] = hiz.HiZ;
                info.Resources->MinMaxDepthReductions[i] = hiz.DepthMinMax;
            }
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
