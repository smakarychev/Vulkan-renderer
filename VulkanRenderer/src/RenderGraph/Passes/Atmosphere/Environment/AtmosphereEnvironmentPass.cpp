#include "AtmosphereEnvironmentPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRaymarchPass.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    RG::Resource environmentMipMap(std::string_view name, RG::Graph& renderGraph, RG::Resource environment)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        struct PassData
        {
            Resource Environment;
        };

        auto& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Environment.MipMap.Setup")

            passData.Environment = graph.Write(environment, Blit);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Environment.MipMap")
            GPU_PROFILE_FRAME("Atmosphere.Environment.MipMap")

            // todo: nvpro mipmap software generation?
            const Texture& cubemap = resources.GetTexture(passData.Environment);
            cubemap.CreateMipmaps(frameContext.Cmd, ImageLayout::Destination);
            DependencyInfo layoutTransition = DependencyInfo::Builder()
                .LayoutTransition({
                    .ImageSubresource = cubemap.Subresource(),
                    .SourceStage = PipelineStage::Blit,
                    .DestinationStage = PipelineStage::Blit,
                    .SourceAccess = PipelineAccess::ReadTransfer,
                    .DestinationAccess = PipelineAccess::WriteTransfer,
                    .OldLayout = ImageLayout::Source,
                    .NewLayout = ImageLayout::Destination})
                .Build(frameContext.DeletionQueue);
            RenderCommand::WaitOnBarrier(frameContext.Cmd, layoutTransition);
        });

        return renderGraph.GetBlackboard().Get<PassData>(pass).Environment;
    }
}

RG::Pass& Passes::Atmosphere::Environment::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource atmosphereSettings, const SceneLight& light, RG::Resource skyViewLut)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Environment.Setup")

            static constexpr bool USE_SUN_LUMINANCE = false;

            std::vector<ImageSubresourceDescription> faceViews(6);
            for (u8 i = 0; i < 6; i++)
                faceViews[i] = ImageSubresourceDescription{
                    .ImageViewKind = ImageViewKind::Image2d, .LayerBase = i, .Layers = 1};    
            
            const u32 environmentSize = (u32)*CVars::Get().GetI32CVar({"Atmosphere.Environment.Size"});
            passData.ColorOut = graph.CreateResource(std::format("{}.ColorOut", name), GraphTextureDescription{
                .Width = environmentSize,
                .Height = environmentSize,
                .Layers = 6,
                .Mipmaps = Image::CalculateMipmapCount({environmentSize, environmentSize}),
                .Format = Format::RGBA16_FLOAT,
                .Kind = ImageKind::Cubemap,
                .AdditionalViews = faceViews});

            auto& globalResources = graph.GetGlobalResources();
            std::vector directions = {
                glm::vec3{ 1.0, 0.0, 0.0},
                glm::vec3{-1.0, 0.0, 0.0},
                glm::vec3{ 0.0, 1.0, 0.0},
                glm::vec3{ 0.0,-1.0, 0.0},
                glm::vec3{ 0.0, 0.0, 1.0},
                glm::vec3{ 0.0, 0.0,-1.0},
            };
            std::vector upVectors = {
                glm::vec3{0.0,-1.0, 0.0},
                glm::vec3{0.0,-1.0, 0.0},
                glm::vec3{0.0, 0.0, 1.0},
                glm::vec3{0.0, 0.0,-1.0},
                glm::vec3{0.0,-1.0, 0.0},
                glm::vec3{0.0,-1.0, 0.0},
            };
            for (u32 face = 0; face < faceViews.size(); face++)
            {
                /* this should not matter at all */
                static constexpr f32 NEAR = 0.1f;
                static constexpr f32 FAR = 1.0f;
                
                Camera camera = Camera::Perspective({
                    .BaseInfo = CameraCreateInfo{
                        .Position = globalResources.PrimaryCamera->GetPosition(),
                        .Orientation = glm::normalize(glm::quatLookAt(directions[face], upVectors[face])),
                        .Near = NEAR,
                        .Far = FAR,
                        .ViewportWidth = environmentSize,
                        .ViewportHeight = environmentSize},
                    .Fov = glm::radians(90.0f)});
                auto& atmosphere = Raymarch::addToGraph(std::format("{}.{}.Raymarch", name, face), graph,
                    atmosphereSettings, camera, light, skyViewLut, {}, {},
                    passData.ColorOut, faceViews[face], {}, USE_SUN_LUMINANCE);
                auto& atmosphereOutput = graph.GetBlackboard().Get<Raymarch::PassData>(atmosphere);
                passData.ColorOut = atmosphereOutput.ColorOut;
            }

            passData.ColorOut = environmentMipMap(std::format("{}.Mipmaps", name), renderGraph, passData.ColorOut);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
