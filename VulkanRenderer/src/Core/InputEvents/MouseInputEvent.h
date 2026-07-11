#pragma once

#include "InputEvent.h"
#include "Core/KeyCodes.h"

namespace lux
{
struct MouseMovedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Mouse, InputEventType::MouseMoved)
    
    MouseMovedEvent(f32 x, f32 y)
        : InputEvent(GetStaticSource(), GetStaticType()), X(x), Y(y)
    {
    }
    f32 X{};
    f32 Y{};
};
struct MouseButtonPressedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Mouse, InputEventType::MouseButtonPressed)
    
    MouseButtonPressedEvent(MouseButton button) : InputEvent(GetStaticSource(), GetStaticType()), Button(button)
    {
    }
    
    MouseButton Button{};
};
struct MouseButtonReleasedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Mouse, InputEventType::MouseButtonReleased)
    
    MouseButtonReleasedEvent(MouseButton button) : InputEvent(GetStaticSource(), GetStaticType()), Button(button)
    {
    }
    
    MouseButton Button{};
};
struct MouseScrolledEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Mouse, InputEventType::MouseScrolled)
    
    MouseScrolledEvent(f32 dy) : InputEvent(GetStaticSource(), GetStaticType()), Dy(dy)
    {
    }
    f32 Dy{};
};
}
