#pragma once
#include <glm/vec2.hpp>

#include "KeyCodes.h"

class Input
{
public:
    static bool GetKey(KeyCode keycode);

    static bool GetMouseButton(MouseCode mousecode);

    static glm::vec2 MousePosition();

public:
    static glm::vec2 s_MainViewportOffset;
    static glm::vec2 s_MainViewportSize;
};
