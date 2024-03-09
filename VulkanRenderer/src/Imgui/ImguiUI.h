#pragma once

#include <memory>
#include <imgui/imgui.h>

class RenderingInfo;
class CommandBuffer;

class ImGuiUI
{
public:
    static void Init(void* window);
    static void Shutdown();

    static void BeginFrame();
    static void EndFrame(const CommandBuffer& cmd, const RenderingInfo& renderingInfo);
    
private:
    struct Payload;
    static std::unique_ptr<Payload> s_Payload;
};
