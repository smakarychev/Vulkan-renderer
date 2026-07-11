#include "rendererpch.h"

#include "Input.h"

#include "Renderer.h"
#include "InputEvents/InputEventDispatcher.h"
#include "InputEvents/KeyboardInputEvent.h"
#include "InputEvents/MouseInputEvent.h"
#include "Window/Window.h"

glm::vec2 Input::s_MainViewportOffset{};
glm::vec2 Input::s_MainViewportSize{};

glm::vec2 Input::s_MousePosition{};

std::bitset<Key::KEY_COUNT> Input::s_PressedKeys{};
std::bitset<Key::KEY_COUNT> Input::s_ReleasedKeys{};
std::bitset<Mouse::BUTTON_COUNT> Input::s_PressedButtons{};
std::bitset<Mouse::BUTTON_COUNT> Input::s_ReleasedButtons{};

bool Input::IsKeyPressed(KeyCode key)
{
    return Renderer::Get()->GetWindow().PollKey(key);
}

bool Input::IsKeyJustPressed(KeyCode key)
{
    return s_PressedKeys[key];
}

bool Input::IsKeyJustReleased(KeyCode key)
{
    return s_ReleasedKeys[key];
}

bool Input::IsMouseButtonJustPressed(MouseButton button)
{
    return s_PressedButtons[button];
}

bool Input::IsMouseButtonJustReleased(MouseButton button)
{ 
    return s_ReleasedButtons[button];
}

bool Input::IsMouseButtonPressed(MouseButton button)
{
    return Renderer::Get()->GetWindow().PollButton(button);
}

glm::vec2 Input::MousePosition()
{
    return {
        s_MousePosition.x + s_MainViewportOffset.x,
        s_MainViewportSize.y - ((f32)s_MousePosition.y + s_MainViewportOffset.y)
    };
}

void Input::OnUpdate()
{
    s_PressedKeys = {}; 
    s_ReleasedKeys = {};
    s_PressedButtons = {};
    s_ReleasedButtons = {};
}

void Input::OnInputEvent(const lux::InputEvent& event)
{
    lux::InputEventDispatcher dispatcher(event);
    dispatcher.Dispatch<lux::KeyPressedEvent>([](const lux::KeyPressedEvent& event)
    {
        if (event.Repeat)
            return;
        
        s_PressedKeys[event.Key] = true;
    });
    dispatcher.Dispatch<lux::KeyReleasedEvent>([](const lux::KeyReleasedEvent& event)
    {
        s_ReleasedKeys[event.Key] = true;
    });
    dispatcher.Dispatch<lux::MouseMovedEvent>([](const lux::MouseMovedEvent& event)
    {
        s_MousePosition = {event.X, event.Y};
    });
    dispatcher.Dispatch<lux::MouseButtonPressedEvent>([](const lux::MouseButtonPressedEvent& event)
    {
        s_PressedButtons[event.Button] = true;
    });
    dispatcher.Dispatch<lux::MouseButtonReleasedEvent>([](const lux::MouseButtonReleasedEvent& event)
    {
        s_ReleasedButtons[event.Button] = true;
    });
}

void Input::OnWindowResized(u32 width, u32 height)
{
    s_MainViewportSize = {width, height};
}
