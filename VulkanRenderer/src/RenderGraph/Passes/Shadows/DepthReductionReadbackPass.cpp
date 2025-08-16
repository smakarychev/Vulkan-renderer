#include "DepthReductionReadbackPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/HiZ/HiZBlitPass.h"

namespace 
{
    f32 linearizeDepth(f32 z, const Camera& camera)
    {
        ASSERT(camera.GetType() == CameraType::Perspective, "Only perspective linearization is implemented")

        return -camera.GetNear() / z;
    }
}

Passes::DepthReductionReadback::PassData& Passes::DepthReductionReadback::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DepthReductionReadback.Setup")
            
            graph.HasSideEffect();
            
            passData.MinMaxDepth = graph.ReadBuffer(info.MinMaxDepthReduction, Readback);

            const Buffer minMax = graph.GetBuffer(passData.MinMaxDepth);
            const void* address = Device::MapBuffer(minMax);
            HiZ::MinMaxDepth depths = *(const HiZ::MinMaxDepth*)address;
            Device::UnmapBuffer(minMax);

            passData.Min = -linearizeDepth(std::bit_cast<f32>(depths.Max), *info.PrimaryCamera);
            passData.Max = -linearizeDepth(std::bit_cast<f32>(depths.Min), *info.PrimaryCamera);
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
