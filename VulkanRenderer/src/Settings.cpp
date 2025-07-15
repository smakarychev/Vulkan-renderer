#include "Settings.h"

#include <filesystem>

#include "cvars/CVarSystem.h"
#include "Rendering/Commands/RenderCommands.h"
#include "Scene/ScenePass.h"
#include "Scene/Visibility/SceneVisibility.h"

void Settings::initCvars()
{
    // todo: mark as readonly once i know how to enforce it 
    CVarString assetsPath("Path.Assets"_hsv, "Base path to assets", "../assets/");
    CVarString shadersPath("Path.Shaders"_hsv, "Relative path to shaders", "shaders/");
    CVarString shadersPathFull("Path.Shaders.Full"_hsv, "Full path to shaders", 
        (std::filesystem::path(assetsPath.Get()) / shadersPath.Get()).generic_string());

    CVarI32 resourceUploaderStagingSize("Uploader.StagingSizeBytes"_hsv,
        "The size of staging buffer used to upload data, in bytes",
        16 * 1024 * 1024);
    CVarI32 resourceUploaderStagingLifetime("Uploader.StagingLifetime"_hsv,
        "The lifetime of staging buffer, in frames",
        300);

    /* main rendering settings */
    CVarI32 depthPrepass("Renderer.DepthPrepass"_hsv,
        "Flag if depth prepass enabled (possible values are 0 (disabled) and 1 (enabled, default)", (i32)true);
    CVarI32 forwardShading("Renderer.UseForwardShading"_hsv,
        "Flag if renderer uses forward shading as main rendering pass "
        "(possible values are 0 (disabled, default) and 1 (enabled)", (i32)false);
    CVarI32 atmosphereRendering("Renderer.Atmosphere"_hsv,
        "Flag if renderer should render sky as physically correct atmosphere "
        "(possible values are 0 (disabled) and 1 (enabled, default)", (i32)true);
    
    CVarI32 prefilterMapResolution("Renderer.IBL.PrefilterResolution"_hsv,
        "Resolution of environment prefilter map", 256);
    CVarI32 prefilterMapResolutionRealtime("Renderer.IBL.PrefilterResolutionRealtime"_hsv,
        "Resolution of realtime environment prefilter map", 128);

    /* render graph */
    CVarI32 renderGraphPoolMaxUnreferencedFrame("RG.UnreferencedResourcesLifetime"_hsv,
        "Number of frames unreferenced resource are kept in memory before being freed", 4);


    /* lights */
    CVarI32 maxLightsPerFrustum("Lights.FrustumMax"_hsv,
        "The maximum amount of point light sources per frustum (view)",
        1024);
    CVarI32 binLightTile("Lights.Bin.Tiles"_hsv,
        "Bin lights into tiles (with z-binning)",
        1);
    CVarI32 binLightCluster("Lights.Bin.Clusters"_hsv,
        "Bin lights into clusters",
        0);

    /* atmosphere */
    CVarI32 transmittanceLutWidth("Atmosphere.Transmittance.Width"_hsv,
        "Width of the atmosphere transmittance LUT",
        256);
    CVarI32 transmittanceLutHeight("Atmosphere.Transmittance.Height"_hsv,
        "Height of the atmosphere transmittance LUT",
        64);
    CVarI32 skyViewLutWidth("Atmosphere.SkyView.Width"_hsv,
        "Width of the atmosphere sky-view LUT",
        200);
    CVarI32 skyViewLutHeight("Atmosphere.SkyView.Height"_hsv,
        "Height of the atmosphere sky-view LUT",
        100);
    CVarI32 multiscatteringLutSize("Atmosphere.Multiscattering.Size"_hsv,
        "Size of the atmosphere multiscattering LUT",
        32);
    CVarI32 aerialPerspectiveLutSize("Atmosphere.AerialPerspective.Size"_hsv,
        "Size of the atmosphere aerial perspective LUT",
        32);
    CVarI32 atmosphereEnvironmentSize("Atmosphere.Environment.Size"_hsv,
        "Size of the each face of atmosphere environment skybox",
        128);

    /* clouds */
    CVarI32 cloudMapSize("Clouds.CloudMap.Size"_hsv,
        "Size of the cloud map",
        512);

    CVarI32 cloudCurlNoiseSize("Clouds.CloudCurlNoise.Size"_hsv,
        "Size of the cloud curl noise",
        128);
    

    /* scene */
    static constexpr u32 DEFAULT_RENDER_OBJECT_COUNT = 1024;
    static constexpr u32 DEFAULT_MESHLET_COUNT = 1024 * 64;
    CVarI32 scenePassRenderObjectBucketDrawBufferSize("Scene.Pass.DrawCommands.SizeBytes"_hsv,
        "Default size of the scene pass draw commands buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(IndirectDispatchCommand));
    CVarI32 sceneSetRenderObjectBufferSize("Scene.RenderObjectSet.Buffer.SizeBytes"_hsv,
        "Default size of the scene render object set buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(SceneRenderObjectHandle));
    CVarI32 sceneSetMeshletBufferSize("Scene.RenderObjectSet.MeshletBuffer.SizeBytes"_hsv,
        "Default size of the scene render object set buffer",
        DEFAULT_MESHLET_COUNT * sizeof(SceneMeshletHandle));
    CVarI32 sceneSetRenderObjectBucketBufferSize("Scene.RenderObjectSet.RenderObjectBuckets.SizeBytes"_hsv,
        "Default size of the scene render object set buckets buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(SceneBucketBits));
    CVarI32 sceneVisibilityBufferSize("Scene.Visibility.Buffer.SizeBytes"_hsv,
        "Default size of the scene visibility buffer",
        DEFAULT_RENDER_OBJECT_COUNT / sizeof(SceneVisibilityBucket));
    CVarI32 sceneMeshletVisibilityBufferSize("Scene.Visibility.Meshlet.Buffer.SizeBytes"_hsv,
        "Default size of the scene visibility buffer for meshlets",
        DEFAULT_MESHLET_COUNT / sizeof(SceneVisibilityBucket));
}
