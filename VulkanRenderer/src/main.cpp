#define VOLK_IMPLEMENTATION
#include <volk.h>

#ifdef TRACY_ENABLE
#include "TracyClient.cpp"
#endif

#include "Renderer.h"
#include "cvars/CVarSystem.h"
#include "utils/utils.h"

i32 main()
{
    Settings::initCvars();
    
    Utils::runSubProcess("../tools/AssetConverter/bin/Release-windows-x86_64/AssetConverter/AssetConverter.exe",
        {CVars::Get().GetStringCVar({"Path.Assets"}, "../assets")});

    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
}
