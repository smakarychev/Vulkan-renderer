#include "Settings.h"

#include "cvars/CVarSystem.h"

void Settings::initCvars()
{
    CVarString assetsPath({"Path.Assets"}, "Base path to assets", "../assets");
}
