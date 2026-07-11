#pragma once
#include <glm/vec2.hpp>

#include "KeyCodes.h"

namespace lux
{
class InputEvent;
}

class Input
{
public:
    static bool IsKeyPressed(KeyCode key);
    static bool IsKeyJustPressed(KeyCode key);
    static bool IsKeyJustReleased(KeyCode key);
    static bool IsMouseButtonPressed(MouseButton button);
    static bool IsMouseButtonJustPressed(MouseButton button);
    static bool IsMouseButtonJustReleased(MouseButton button);
    static glm::vec2 MousePosition();

    static void OnUpdate();
    static void OnInputEvent(const lux::InputEvent& event);
    static void OnWindowResized(u32 width, u32 height);
private:
    static glm::vec2 s_MainViewportOffset;
    static glm::vec2 s_MainViewportSize;
    
    static glm::vec2 s_MousePosition;
    
    static std::bitset<Key::KEY_COUNT> s_PressedKeys;
    static std::bitset<Key::KEY_COUNT> s_ReleasedKeys;
    
    static std::bitset<Mouse::BUTTON_COUNT> s_PressedButtons;
    static std::bitset<Mouse::BUTTON_COUNT> s_ReleasedButtons;
};
