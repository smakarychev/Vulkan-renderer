#pragma once

#include "Core/KeyCodes.h"
#include "Core/InputEvents/InputEvent.h"
#include <glm/vec2.hpp>

namespace lux
{
struct WindowSize
{
    u32 Width{1600};
    u32 Height{900};
};
struct WindowParameters
{
    std::string Name{};
    WindowSize Size{};
    OnInputEventUserPointer UserPointer{nullptr};
    OnInputEventFn InputEventFn{[](void*, const InputEvent&){}};
};

enum class WindowSurfaceBackend : u8
{
    Vulkan
};
class WindowSurface
{
public:
    WindowSurface(u64 nativeWindow) : m_NativeWindow(nativeWindow) {}
protected:
    u64 m_NativeWindow{};
};

class Window
{
public:
    Window(const WindowParameters& parameters);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(const Window&&) = delete;
    Window& operator=(Window&&) = delete;
    ~Window();
    
    WindowSize GetWindowSize() const;
    void WaitAnyEvent() const;
    void OnUpdate() const;
    bool ShouldClose() const;
    std::unique_ptr<WindowSurface> CreateSurfaceFor(WindowSurfaceBackend backend) const;
    void InitForImGui() const;
    void ShutdownImGui() const;
    
    bool PollKey(KeyCode key) const;
    bool PollButton(MouseButton button) const;
    glm::vec2 PollMouse() const;
private:
    u64 m_NativeWindow{};
    OnInputEventUserPointer m_UserPointer{nullptr};
    OnInputEventFn m_InputEventFn{nullptr};
};
}
