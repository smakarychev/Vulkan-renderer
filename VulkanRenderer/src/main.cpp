#include "Renderer.h"
#include "cvars/CVarSystem.h"
#include "Platform/PlatformUtils.h"

i32 main()
{
    Settings::initCvars();
    
    Platform::runSubProcess("../tools/bin/AssetConverter.exe",
        {CVars::Get().GetStringCVar("Path.Assets"_hsv, "../assets")});

    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
}
