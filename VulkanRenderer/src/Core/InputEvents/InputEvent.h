#pragma once

#include <CoreLib/types.h>

namespace lux
{
enum class InputEventSource : u8
{
    None = 0,
    Keyboard,
    Mouse,
    Window
};
enum class InputEventType : u8
{
    None = 0,
    KeyPressed, KeyReleased,
    MouseMoved, MouseButtonPressed, MouseButtonReleased, MouseScrolled,
    WindowClosed, WindowResized,
};

class InputEvent
{
public:
    constexpr InputEvent(InputEventSource source, InputEventType type) : m_Source(source), m_Type(type) {}
    
    std::string ToString() const;
    constexpr InputEventSource GetSource() const { return m_Source; }
    constexpr InputEventType GetType() const { return m_Type; }
private:
    InputEventSource m_Source{InputEventSource::None};
    InputEventType m_Type{InputEventType::None};
};

using OnInputEventUserPointer = void*;
using OnInputEventFn = void (*)(OnInputEventUserPointer, const InputEvent& event);

#define DEFINE_INPUT_EVENT(source, type) \
    static constexpr InputEventSource GetStaticSource() { return source; } \
    static constexpr InputEventType GetStaticType() { return type; }
}