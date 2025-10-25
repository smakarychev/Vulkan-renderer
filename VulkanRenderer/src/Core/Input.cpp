#include "rendererpch.h"

#include "Input.h"

#include "Renderer.h"

#include <GLFW/glfw3.h>

glm::vec2 Input::s_MainViewportOffset   = glm::vec2(0.0f, 0.0f);
glm::vec2 Input::s_MainViewportSize     = glm::vec2(1600.0f, 900.0f);

bool Input::GetKey(KeyCode keycode)
{
    GLFWwindow* window = static_cast<GLFWwindow*>(Renderer::Get()->GetWindow());
    i32 state = glfwGetKey(window, static_cast<i32>(keycode));
    return state == GLFW_PRESS;
}

bool Input::GetMouseButton(MouseCode mousecode)
{
    GLFWwindow* window = static_cast<GLFWwindow*>(Renderer::Get()->GetWindow());
    i32 state = glfwGetMouseButton(window, static_cast<i32>(mousecode));
    return state == GLFW_PRESS;
}

glm::vec2 Input::MousePosition()
{
    GLFWwindow* window = static_cast<GLFWwindow*>(Renderer::Get()->GetWindow());
    f64 xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    return { (f32)xpos + s_MainViewportOffset.x,  s_MainViewportSize.y - ((f32)ypos + s_MainViewportOffset.y) };
}
