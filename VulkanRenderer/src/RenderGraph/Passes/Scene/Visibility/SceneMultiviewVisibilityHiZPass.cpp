#include "SceneMultiviewVisibilityHiZPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/HiZ/HiZFullPass.h"
#include "RenderGraph/Passes/HiZ/HiZNVPass.h"

RG::Pass& Passes::SceneMultiviewVisibilityHiz::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
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
                    enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::OcclusionCull) &&
                    !info.HiZs[i].IsValid();
                if (!requireHiZ)
                    continue;

                const bool isPrimaryView = enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::IsPrimaryView);
                if (isPrimaryView)
                {
                    // todo: store min max depth for shadows
                    auto& hizPass = HiZFull::addToGraph(name.AddVersion(i), graph, {
                        .Depth = info.Depths[i],
                        .Subresource = info.Subresources[i]});
                    auto& hizOutput = graph.GetBlackboard().Get<HiZFull::PassData>(hizPass);
                    info.Resources->Hiz[i] = hizOutput.HiZMin;
                }
                else
                {
                    auto& hizPass = HiZNV::addToGraph(name.AddVersion(i), graph, {
                        .Depth = info.Depths[i],
                        .Subresource = info.Subresources[i]});
                    auto& hizOutput = graph.GetBlackboard().Get<HiZNV::PassData>(hizPass);
                    info.Resources->Hiz[i] = hizOutput.HiZ;
                }
            }
        },
        [=](PassData&, FrameContext&, const Resources&)
        {
        });
}
