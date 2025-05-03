#include "DepthReductionReadbackPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/HiZ/HiZBlitPass.h"

namespace 
{
    f32 linearizeDepth(f32 z, const Camera& camera)
    {
        ASSERT(camera.GetType() == CameraType::Perspective, "Only perspective linearization is implemented")
        f32 n = camera.GetFrustumPlanes().Near;
        f32 f = camera.GetFrustumPlanes().Far;

        return f * n / ((n - f) * z - n);
    }
}

RG::Pass& Passes::DepthReductionReadback::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource minMaxDepth, const Camera* primaryCamera)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DepthReductionReadback.Setup")
            
            graph.HasSideEffect();
            
            graph.SetShader("create-shadow-cameras"_hsv);

            passData.MinMaxDepth = graph.Read(minMaxDepth, Readback);

            Buffer minMax = Resources{graph}.GetBuffer(passData.MinMaxDepth);
            const void* address = Device::MapBuffer(minMax);
            HiZ::MinMaxDepth depths = *(const HiZ::MinMaxDepth*)address;
            Device::UnmapBuffer(minMax);

            passData.Min = -linearizeDepth(std::bit_cast<f32>(depths.Max), *primaryCamera);
            passData.Max = -linearizeDepth(std::bit_cast<f32>(depths.Min), *primaryCamera);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
