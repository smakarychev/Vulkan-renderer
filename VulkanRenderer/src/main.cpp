#include "Renderer.h"
#include "utils/StringHasher.h"
#include "utils/utils.h"
#include "Core/core.h"

i32 main()
{
    Utils::runSubProcess("../tools/AssetConverter/bin/Release-windows-x86_64/AssetConverter/AssetConverter.exe", {"../assets"});

    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
}
