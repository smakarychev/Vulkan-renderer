#include "Application.h"
#include "Renderer.h"

void CheckNewAPI();

int main()
{
    CheckNewAPI();
    
    Application app;
    app.Run();
}

void CheckNewAPI()
{
    Renderer renderer;
    renderer.Run();
}
