#include "Renderer.h"
#include "utils.h"
#include "Vulkan\Shader.h"

int main()
{
    utils::runSubProcess("../tools/AssetConverter/bin/Release-windows-x86_64/AssetConverter/AssetConverter.exe", {"../assets"});
    Renderer renderer;
    renderer.Run();
}
