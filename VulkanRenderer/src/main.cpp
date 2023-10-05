#include "Renderer.h"
#include "utils\utils.h"

int main()
{
    utils::runSubProcess("../tools/AssetConverter/bin/Release-windows-x86_64/AssetConverter/AssetConverter.exe", {"../assets"});
    Renderer* renderer = Renderer::Get();
    renderer->Init();
    renderer->Run();
}
