#pragma once

class GLFWwindow;

class Application
{
public:
    void Run();
private:
    void Init();
    void MainLoop();
    void CleanUp();
private:
    GLFWwindow* m_Window{nullptr};
};
