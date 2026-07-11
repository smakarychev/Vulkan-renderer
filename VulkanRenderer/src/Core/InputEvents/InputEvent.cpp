#include "rendererpch.h"
#include "InputEvent.h"

#include "KeyboardInputEvent.h"
#include "MouseInputEvent.h"
#include "WindowInputEvent.h"

namespace lux
{
std::string InputEvent::ToString() const
{
    switch (m_Type) {
    case InputEventType::None:
        return "NoneEvent";
    case InputEventType::KeyPressed:
        {
            auto& keyPressed = (const KeyPressedEvent&)*this;
            return std::format("KeyPressedEvent<{} (repeat: {})>", 
                Key::keyCodeToString(keyPressed.Key), keyPressed.Repeat ? "true" : "false");
        }
    case InputEventType::KeyReleased:
        {
            auto& keyReleased = (const KeyReleasedEvent&)*this;
            return std::format("KeyReleasedEvent<{}>", Key::keyCodeToString(keyReleased.Key));
        }
    case InputEventType::MouseMoved:
        {
            auto& keyReleased = (const KeyReleasedEvent&)*this;
            return std::format("KeyReleasedEvent<{}>", Key::keyCodeToString(keyReleased.Key));
        }
    case InputEventType::MouseButtonPressed:
        {
            auto& mousePressed = (const MouseButtonPressedEvent&)*this;
            return std::format("MouseButtonPressedEvent<{}>", Mouse::mouseCodeToString(mousePressed.Button));
        }
    case InputEventType::MouseButtonReleased:
        {
            auto& mouseReleased = (const MouseButtonReleasedEvent&)*this;
            return std::format("MouseButtonReleasedEvent<{}>", Mouse::mouseCodeToString(mouseReleased.Button));
        }
    case InputEventType::MouseScrolled:
        {
            auto& mouseScrolled = (const MouseScrolledEvent&)*this;
            return std::format("MouseScrolledEvent<{}>", mouseScrolled.Dy);
        }
    case InputEventType::WindowClosed:
        return std::format("WindowClosedEvent");
    case InputEventType::WindowResized:
        {
            auto& windowResized = (const WindowResizedEvent&)*this;
            return std::format("WindowResizedEvent<{} {}>", windowResized.Width, windowResized.Height);
        }
    }
    
    return "UnknownEvent";
}
}
