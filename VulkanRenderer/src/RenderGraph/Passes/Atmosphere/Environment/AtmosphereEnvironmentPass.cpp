#include "rendererpch.h"

#include "AtmosphereEnvironmentPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRenderPass.h"
#include "Rendering/Image/ImageUtility.h"

Passes::Atmosphere::Environment::PassData& Passes::Atmosphere::Environment::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Environment.Setup")

            const f32 environmentSize = (f32)*CVars::Get().GetI32CVar("Atmosphere.Environment.Size"_hsv);

            if (info.ColorIn.IsValid())
                passData.Color = info.ColorIn;
            else 
                passData.Color = graph.Create("EnvironmentColorOut"_hsv, RGImageDescription{
                    .Width = environmentSize,
                    .Height = environmentSize,
                    .LayersDepth = 6,
                    .Mipmaps = Images::mipmapCount({environmentSize, environmentSize}),
                    .Format = Format::RGBA16_FLOAT,
                    .Kind = ImageKind::ImageCubemap
                });

            std::array<Resource, 6> faces{};

            for (u32 i = 0; i < info.FaceIndices.size(); i++)
            {
                const u32 faceIndex = info.FaceIndices[i];
                const Camera camera = Camera::EnvironmentCapture(info.PrimaryView->Camera.Position,
                    (u32)environmentSize, faceIndex);
                ViewInfoGPU viewInfo = *info.PrimaryView;
                viewInfo.Camera = CameraGPU::FromCamera(camera, {environmentSize, environmentSize});
                Resource viewInfoResource = graph.Create("ViewInfo"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(ViewInfoGPU)});
                viewInfoResource = graph.Upload(viewInfoResource, viewInfo);

                faces[i] = graph.SplitImage(passData.Color,
                    {.ImageViewKind = ImageViewKind::Image2d, .LayerBase = (i8)faceIndex, .Layers = 1});
                
                auto& atmosphere = Render::addToGraph(
                    name.Concatenate(".Render").AddVersion(faceIndex), graph, {
                        .ViewInfo = viewInfoResource,
                        .SkyViewLut = info.SkyViewLut,
                        .ColorIn = faces[i],
                        .IsPrimaryView = false
                    });
                faces[i] = atmosphere.Color;
            }

            passData.Color = graph.MergeImage(Span<const Resource>(faces.data(), info.FaceIndices.size()));
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
