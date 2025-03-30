#include "Settings.h"

#include <filesystem>

#include "cvars/CVarSystem.h"
#include "Rendering/Commands/RenderCommands.h"
#include "Scene/ScenePass.h"
#include "Scene/SceneVisibility.h"

void Settings::initCvars()
{
    // todo: mark as readonly once i know how to enforce it 
    CVarString assetsPath({"Path.Assets"}, "Base path to assets", "../assets/");
    CVarString shadersPath({"Path.Shaders"}, "Relative path to shaders", "shaders/");
    CVarString shadersPathFull({"Path.Shaders.Full"}, "Full path to shaders", 
        (std::filesystem::path(assetsPath.Get()) / shadersPath.Get()).generic_string());

    CVarI32 resourceUploaderStagingSize({"Uploader.StagingSizeBytes"},
        "The size of staging buffer used to upload data, in bytes",
        16 * 1024 * 1024);
    CVarI32 resourceUploaderStagingLifetime({"Uploader.StagingLifetime"},
        "The lifetime of staging buffer, in frames",
        300);


    /* lights */
    CVarI32 maxLightsPerFrustum({"Lights.FrustumMax"},
        "The maximum amount of point light sources per frustum (view)",
        1024);
    CVarI32 binLightTile({"Lights.Bin.Tiles"},
        "Bin lights into tiles (with z-binning)",
        1);
    CVarI32 binLightCluster({"Lights.Bin.Clusters"},
        "Bin lights into clusters",
        0);

    /* atmosphere */
    CVarI32 transmittanceLutWidth({"Atmosphere.Transmittance.Width"},
        "Width of the atmosphere transmittance LUT",
        256);
    CVarI32 transmittanceLutHeight({"Atmosphere.Transmittance.Height"},
        "Height of the atmosphere transmittance LUT",
        64);
    CVarI32 skyViewLutWidth({"Atmosphere.SkyView.Width"},
        "Width of the atmosphere sky-view LUT",
        200);
    CVarI32 skyViewLutHeight({"Atmosphere.SkyView.Height"},
        "Height of the atmosphere sky-view LUT",
        100);
    CVarI32 multiscatteringLutSize({"Atmosphere.Multiscattering.Size"},
        "Size of the atmosphere multiscattering LUT",
        32);
    CVarI32 aerialPerspectiveLutSize({"Atmosphere.AerialPerspective.Size"},
        "Size of the atmosphere aerial perspective LUT",
        32);
    CVarI32 atmosphereEnvironmentSize({"Atmosphere.Environment.Size"},
        "Size of the each face of atmosphere environment skybox",
        128);

    /* scene */
    static constexpr u32 DEFAULT_RENDER_OBJECT_COUNT = 1024;
    CVarI32 scenePassRenderObjectBucketDrawBufferSize({"Scene.Pass.DrawCommands.SizeBytes"},
        "Default size of the scene pass draw commands buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(IndirectDispatchCommand));
    CVarI32 sceneSetRenderObjectBufferSize({"Scene.RenderObjectSet.Buffer.SizeBytes"},
        "Default size of the scene render object set buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(SceneRenderObjectHandle));
    CVarI32 sceneSetRenderObjectBucketBufferSize({"Scene.RenderObjectSet.RenderObjectBuckets.SizeBytes"},
        "Default size of the scene render object set buckets buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(SceneBucketBits));
    CVarI32 sceneSetMeshletBufferSize({"Scene.RenderObjectSet.MeshletSpan.SizeBytes"},
        "Default size of the scene meshlet span buffer",
        DEFAULT_RENDER_OBJECT_COUNT * sizeof(RenderObjectMeshletSpan));
    CVarI32 sceneVisibilityBufferSize({"Scene.Visibility.Buffer.SizeBytes"},
        "Default size of the scene visibility buffer",
        DEFAULT_RENDER_OBJECT_COUNT / sizeof(SceneVisibilityBucket));
}
