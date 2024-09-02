#include "Settings.h"

#include <filesystem>

#include "cvars/CVarSystem.h"

void Settings::initCvars()
{
    // todo: mark as readonly once i know how to enforce it 
    CVarString assetsPath({"Path.Assets"}, "Base path to assets", "../assets");
    CVarString shadersPath({"Path.Shaders"}, "Relative path to shaders", "shaders");
    CVarString shadersPathFull({"Path.Shaders.Full"}, "Full path to shaders", 
        (std::filesystem::path(assetsPath.Get()) / shadersPath.Get()).generic_string());
}
