#pragma once

#include "InputEvent.h"

namespace lux
{
struct WindowClosedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Window, InputEventType::WindowClosed)
    
    WindowClosedEvent() : InputEvent(GetStaticSource(), GetStaticType())
    {
    }
};
struct WindowResizedEvent : InputEvent
{
    DEFINE_INPUT_EVENT(InputEventSource::Window, InputEventType::WindowResized)
    
    WindowResizedEvent(u32 width, u32 height)
        : InputEvent(GetStaticSource(), GetStaticType()), Width(width), Height(height)
    {
    }
    u32 Width{};
    u32 Height{};
};
}
