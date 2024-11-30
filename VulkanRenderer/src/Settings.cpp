#include "Settings.h"

#include <filesystem>

#include "cvars/CVarSystem.h"

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
}
