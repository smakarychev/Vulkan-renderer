#include "AtmosphereEnvironmentPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Atmosphere/AtmosphereRaymarchPass.h"
#include "RenderGraph/Passes/Utility/MipMapPass.h"
#include "Vulkan/RenderCommand.h"

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
            for (i8 i = 0; i < 6; i++)
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
                        .ViewportHeight = environmentSize,
                        .FlipY = false},
                    .Fov = glm::radians(90.0f)});
                
                auto& atmosphere = Raymarch::addToGraph(std::format("{}.{}.Raymarch", name, face), graph,
                    atmosphereSettings, camera, light, skyViewLut, {}, {},
                    passData.ColorOut, faceViews[face], {}, USE_SUN_LUMINANCE);
                auto& atmosphereOutput = graph.GetBlackboard().Get<Raymarch::PassData>(atmosphere);
                passData.ColorOut = atmosphereOutput.ColorOut;
            }

            auto& mipmapped = Mipmap::addToGraph(std::format("{}.Mipmaps", name), graph, passData.ColorOut);
            passData.ColorOut = graph.GetBlackboard().Get<Mipmap::PassData>(mipmapped).Texture;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
