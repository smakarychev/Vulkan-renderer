#include "rendererpch.h"
#include "Window.h"

#include "Core/InputEvents/KeyboardInputEvent.h"
#include "Core/InputEvents/MouseInputEvent.h"
#include "Core/InputEvents/WindowInputEvent.h"
#include "CoreLib/core.h"

#include <imgui/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

namespace lux
{
Window::Window(const WindowParameters& parameters)
    : m_UserPointer(parameters.UserPointer), m_InputEventFn(parameters.InputEventFn)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create opengl context
    GLFWwindow* window = glfwCreateWindow((i32)parameters.Size.Width, (i32)parameters.Size.Height,
        parameters.Name.c_str(), nullptr, nullptr);
    m_NativeWindow = (u64)window;
    glfwSetWindowUserPointer(window, this);
    
    glfwSetKeyCallback(window, [](GLFWwindow* window, i32 key, i32, i32 action, i32)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        switch (action)
        {
        case GLFW_PRESS:
            thisWindow->m_InputEventFn(thisWindow->m_UserPointer, KeyPressedEvent((KeyCode)key, false));
            break;
        case GLFW_REPEAT:
            thisWindow->m_InputEventFn(thisWindow->m_UserPointer, KeyPressedEvent((KeyCode)key, true));
            break;
        case GLFW_RELEASE:
            thisWindow->m_InputEventFn(thisWindow->m_UserPointer, KeyReleasedEvent((KeyCode)key));
            break;
        default:
            ASSERT(false, "Unknown key action type")
            break;
        }
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, f64 x, f64 y)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        thisWindow->m_InputEventFn(thisWindow->m_UserPointer, MouseMovedEvent((f32)x, (f32)y));
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, i32 button, i32 action, i32)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        switch (action)
        {
        case GLFW_PRESS:
            thisWindow->m_InputEventFn(thisWindow->m_UserPointer, MouseButtonPressedEvent((MouseButton)button));
            break;
        case GLFW_RELEASE:
            thisWindow->m_InputEventFn(thisWindow->m_UserPointer, MouseButtonReleasedEvent((MouseButton)button));
            break;
        default:
            ASSERT(false, "Unknown mouse button action type")
            break;
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, f64, f64 dy)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        thisWindow->m_InputEventFn(thisWindow->m_UserPointer, MouseScrolledEvent((f32)dy));
    });
    glfwSetWindowCloseCallback(window, [](GLFWwindow* window)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        thisWindow->m_InputEventFn(thisWindow->m_UserPointer, WindowClosedEvent());
    });
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, i32 width, i32 height)
    {
        const auto thisWindow = (Window*)glfwGetWindowUserPointer(window);
        thisWindow->m_InputEventFn(thisWindow->m_UserPointer, WindowResizedEvent((u32)width, (u32)height));
    });
}

Window::~Window()
{
    glfwDestroyWindow((GLFWwindow*)m_NativeWindow);
}

WindowSize Window::GetWindowSize() const
{
    i32 width, height;
    glfwGetFramebufferSize((GLFWwindow*)m_NativeWindow, &width, &height);
    
    return {.Width = (u32)width, .Height = (u32)height};
}

void Window::WaitAnyEvent() const
{
    glfwWaitEvents();
}

void Window::OnUpdate() const
{
    glfwPollEvents();
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose((GLFWwindow*)m_NativeWindow);
}

std::unique_ptr<WindowSurface> Window::CreateSurfaceFor(WindowSurfaceBackend backend) const
{
    if (backend == WindowSurfaceBackend::Vulkan)
        return std::make_unique<WindowSurface>(m_NativeWindow);
    
    return nullptr;
}

void Window::InitForImGui() const
{
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)m_NativeWindow, true);
}

void Window::ShutdownImGui() const
{
    ImGui_ImplGlfw_Shutdown();
}

bool Window::PollKey(KeyCode key) const
{
    return glfwGetKey((GLFWwindow*)m_NativeWindow, key) == GLFW_PRESS;
}

bool Window::PollButton(MouseButton button) const
{
    return glfwGetMouseButton((GLFWwindow*)m_NativeWindow, button) == GLFW_PRESS;
}

glm::vec2 Window::PollMouse() const
{
    f64 xpos, ypos;
    glfwGetCursorPos((GLFWwindow*)m_NativeWindow, &xpos, &ypos);
    
    return {(f32)xpos, (f32)ypos};
}
}
