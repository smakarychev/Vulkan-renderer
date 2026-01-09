#pragma once
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGResource.h"

namespace Passes::Upload
{
struct PassData
{
    RG::Resource Resource{};
};

template <typename T>
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, T&& data)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto&& [address, sizeBytes] = UploadUtils::getAddressAndSize(data);
            passData.Resource = graph.Create("Resource"_hsv, RGBufferDescription{
                .SizeBytes = sizeBytes
            });
            passData.Resource = graph.Upload(passData.Resource, std::forward<T>(data));
            graph.HasSideEffect();
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        }).Resource;
}
}
