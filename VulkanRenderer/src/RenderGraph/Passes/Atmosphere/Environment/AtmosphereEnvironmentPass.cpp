#include "AtmosphereEnvironmentPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRaymarchPass.h"
#include "RenderGraph/Passes/Utility/MipMapPass.h"
#include "Rendering/Image/ImageUtility.h"

Passes::Atmosphere::Environment::PassData& Passes::Atmosphere::Environment::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Environment.Setup")

            static constexpr bool USE_SUN_LUMINANCE = false;

            const f32 environmentSize = (f32)*CVars::Get().GetI32CVar("Atmosphere.Environment.Size"_hsv);

            if (info.ColorIn.IsValid())
                passData.ColorOut = info.ColorIn;
            else 
                passData.ColorOut = graph.Create("EnvironmentColorOut"_hsv, RGImageDescription{
                    .Width = environmentSize,
                    .Height = environmentSize,
                    .LayersDepth = 6,
                    .Mipmaps = Images::mipmapCount({environmentSize, environmentSize}),
                    .Format = Format::RGBA16_FLOAT,
                    .Kind = ImageKind::Cubemap
                });

            std::array<Resource, 6> faces{};
            
            for (u32 faceIndex = 0; faceIndex < faces.size(); faceIndex++)
            {
                const Camera camera = Camera::EnvironmentCapture(info.PrimaryView->Camera.Position,
                    (u32)environmentSize, faceIndex);
                ViewInfoGPU viewInfo = *info.PrimaryView;
                viewInfo.Camera = CameraGPU::FromCamera(camera, {environmentSize, environmentSize});
                Resource viewInfoResource = graph.Create("ViewInfo"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(ViewInfoGPU)});
                viewInfoResource = graph.Upload(viewInfoResource, viewInfo);

                faces[faceIndex] = graph.SplitImage(passData.ColorOut,
                    {.ImageViewKind = ImageViewKind::Image2d, .LayerBase = (i8)faceIndex, .Layers = 1});
                
                auto& atmosphere = Raymarch::addToGraph(
                    name.Concatenate(".Raymarch").AddVersion(faceIndex), graph, {
                        .ViewInfo = viewInfoResource,
                        .Light = info.Light,
                        .SkyViewLut = info.SkyViewLut,
                        .ColorIn = faces[faceIndex],
                        .UseSunLuminance = USE_SUN_LUMINANCE
                    });
                faces[faceIndex] = atmosphere.ColorOut;
            }

            passData.ColorOut = graph.AddRenderPass<Resource>("EnvironmentMipmaps"_hsv,
                [&](Graph& mipmapGraph, Resource& mipmapData)
                {
                    mipmapData = mipmapGraph.MergeImage(faces);
                    auto& mipmapped = Mipmap::addToGraph(name.Concatenate(".Mipmaps"), graph, mipmapData);
                    mipmapData = mipmapped.Texture;
                },
                [=](const Resource&, FrameContext&, const Graph&){});
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
