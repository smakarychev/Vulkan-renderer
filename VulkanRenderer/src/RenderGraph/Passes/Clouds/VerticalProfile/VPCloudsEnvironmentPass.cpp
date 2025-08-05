#include "VPCloudsEnvironmentPass.h"

#include "ViewInfoGPU.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Clouds/CloudsCommon.h"

Passes::Clouds::VP::Environment::PassData& Passes::Clouds::VP::Environment::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("VP.Environment.Setup")

            f32 environmentSize = (f32)*CVars::Get().GetI32CVar("Atmosphere.Environment.Size"_hsv);
            if (info.CloudsRenderingMode == CloudsRenderingMode::Reprojection)
                environmentSize *= REPROJECTION_RELATIVE_SIZE;
            passData.ColorOut = info.ColorIn;

            const Camera camera = Camera::EnvironmentCapture(info.PrimaryView->Camera.Position,
                    (u32)environmentSize, info.FaceIndex);
            ViewInfoGPU viewInfo = *info.PrimaryView;
            viewInfo.Camera = CameraGPU::FromCamera(camera, {environmentSize, environmentSize});
            Resource viewInfoResource = graph.Create("ViewInfo"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(ViewInfoGPU)});
            viewInfoResource = graph.Upload(viewInfoResource, viewInfo);

            Resource colorFace = graph.SplitImage(passData.ColorOut,
                {.ImageViewKind = ImageViewKind::Image2d, .LayerBase = (i8)info.FaceIndex, .Layers = 1});
            
            auto& clouds = VP::addToGraph(
                name.Concatenate(".Clouds").AddVersion(info.FaceIndex), graph, {
                    .ViewInfo = viewInfoResource,
                    .CloudCoverage = info.CloudCoverage,
                    .CloudProfile = info.CloudProfile,
                    .CloudShapeLowFrequencyMap = info.CloudShapeLowFrequencyMap,
                    .CloudShapeHighFrequencyMap = info.CloudShapeHighFrequencyMap,
                    .CloudCurlNoise = info.CloudCurlNoise,
                    .ColorOut = colorFace,
                    .IrradianceSH = info.IrradianceSH,
                    .Light = info.Light,
                    .CloudParameters = info.CloudParameters,
                    .CloudsRenderingMode = info.CloudsRenderingMode,
                    .IsEnvironmentCapture = true
                });
            colorFace = clouds.ColorOut;
            passData.ColorOut = graph.MergeImage({colorFace});
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
