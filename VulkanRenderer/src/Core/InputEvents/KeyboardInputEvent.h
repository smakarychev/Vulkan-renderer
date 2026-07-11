#pragma once

#include "InputEvent.h"
#include "Core/KeyCodes.h"

namespace lux
{
struct KeyPressedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Keyboard, InputEventType::KeyPressed)
    
    KeyPressedEvent(KeyCode key, bool repeat) : InputEvent(GetStaticSource(), GetStaticType()), Key(key), Repeat(repeat)
    {
    }
    KeyCode Key{};
    bool Repeat{};
};
struct KeyReleasedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Keyboard, InputEventType::KeyReleased)
    
    KeyReleasedEvent(KeyCode key) : InputEvent(GetStaticSource(), GetStaticType()), Key(key)
    {
    }
    KeyCode Key{};
};
}
