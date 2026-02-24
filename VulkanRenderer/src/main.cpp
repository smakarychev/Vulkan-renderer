#include "rendererpch.h"

#include "Log.h"
#include "Renderer.h"
#include "cvars/CVarSystem.h"
#include "Platform/PlatformUtils.h"

i32 main()
{
    lux::Logger::Init({.LogFile = "../Logs/log.txt"});
    Settings::initCvars();
    
    platform::runSubProcess("../tools/bin/AssetConverter/AssetConverter.exe",
        {CVars::Get().GetStringCVar("Path.Assets"_hsv, "../assets")});

    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
    renderer->Shutdown();
}
