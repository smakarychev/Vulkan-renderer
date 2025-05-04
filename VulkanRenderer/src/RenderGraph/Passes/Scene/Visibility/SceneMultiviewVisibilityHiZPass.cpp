#include "SceneMultiviewVisibilityHiZPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/HiZ/HiZFullPass.h"
#include "RenderGraph/Passes/HiZ/HiZNVPass.h"

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
                    enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::OcclusionCull) &&
                    !info.Resources->Hiz[i].IsValid();
                if (!requireHiZ)
                    continue;

                const bool isPrimaryView = enumHasAny(view.VisibilityFlags, SceneVisibilityFlags::IsPrimaryView);
                if (isPrimaryView)
                {
                    // todo: store min max depth for shadows
                    auto& hizPass = HiZFull::addToGraph(name.AddVersion(i), graph, {
                        .Depth = info.Depths[i],
                        .Subresource = info.Subresources[i]});
                    info.Resources->Hiz[i] = hizPass.HiZMin;
                }
                else
                {
                    auto& hizPass = HiZNV::addToGraph(name.AddVersion(i), graph, {
                        .Depth = info.Depths[i],
                        .Subresource = info.Subresources[i]});
                    info.Resources->Hiz[i] = hizPass.HiZ;
                }
            }
        },
        [=](PassData&, FrameContext&, const Resources&)
        {
        }).Data;
}
