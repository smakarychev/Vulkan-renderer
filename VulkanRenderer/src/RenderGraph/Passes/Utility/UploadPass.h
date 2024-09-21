#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGResource.h"

namespace Passes::Upload
{
    struct PassData
    {
        RG::Resource Resource{};
    };
    template <typename T>
    RG::Resource addToGraph(std::string_view name, RG::Graph& renderGraph, T&& data);

    template <typename T>
    RG::Resource addToGraph(std::string_view name, RG::Graph& renderGraph, T&& data)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        Pass& pass = renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                auto&& [address, sizeBytes] = UploadUtils::getAddressAndSize(std::forward<T>(data));
                passData.Resource = graph.CreateResource(std::format("{}.Resource", name), GraphBufferDescription{
                    .SizeBytes = sizeBytes});
                graph.Write(passData.Resource, Copy);
                graph.Upload(passData.Resource, std::forward<T>(data));
                graph.HasSideEffect();

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
            });
        
        return renderGraph.GetBlackboard().Get<PassData>(pass).Resource;
    }
}

