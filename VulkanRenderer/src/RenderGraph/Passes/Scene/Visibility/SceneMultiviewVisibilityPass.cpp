#include "SceneMultiviewVisibilityPass.h"

#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

RG::Pass& Passes::SceneMultiviewVisibility::addToGraph(StringId name, RG::Graph& renderGraph,
    ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassDataPrivate
    {
        std::vector<Pass*> DrawPasses{};
        std::vector<SceneBaseDrawPassData*> DrawPassesData{};

        Resource RenderObjectVisibility{};
        Resource MeshletVisibility{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name, 
        [&](Graph& graph, PassDataPrivate& passData)
        {
            passData.DrawPasses.reserve(info.BucketPasses.size());
            passData.DrawPassesData.reserve(info.BucketPasses.size());
            for (auto&& [bucket, init] : info.BucketPasses)
            {
                auto&& [drawPass, drawPassData] = init(name);
                passData.DrawPasses.push_back(drawPass);
                passData.DrawPassesData.push_back(drawPassData);
            }
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            
        });
}
