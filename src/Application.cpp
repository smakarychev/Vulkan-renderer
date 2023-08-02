#include "Application.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

void Application::Run()
{
    Init();
    MainLoop();
    CleanUp();
}

void Application::Init()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not crete opengl context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // resizing in vulkan is not trivial
}

void Application::MainLoop()
{
}

void Application::CleanUp()
{
}
