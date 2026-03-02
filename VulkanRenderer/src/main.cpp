#include "rendererpch.h"

#include "Log.h"
#include "Renderer.h"
#include "cvars/CVarSystem.h"
#include "Platform/PlatformUtils.h"

i32 main()
{
    lux::Logger::Init({.LogFile = "../logs/log.txt"});
    Settings::initCvars();
    
    platform::runSubProcess("../tools/bin/AssetBaker/AssetBaker.exe",
        {CVars::Get().GetStringCVar("Path.Assets"_hsv, "../assets")});

    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
    renderer->Shutdown();
}
