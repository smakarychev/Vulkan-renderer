﻿#include "Settings.h"

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

    
}