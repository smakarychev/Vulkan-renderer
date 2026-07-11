#pragma once
#include "InputEvent.h"

namespace lux
{
template <typename Event, typename Fn>
concept InputEventDispatchable = 
    std::derived_from<Event, InputEvent> &&
    requires(Fn fn, Event e)
    {
        { fn(e) } -> std::same_as<void>;
    };

class InputEventDispatcher
{
public:
    InputEventDispatcher(const InputEvent& event) : m_Event(&event) {}
    
    template <typename Event, typename Fn>
    requires InputEventDispatchable<Event, Fn>
    void Dispatch(Fn&& handler)
    {
        if (m_IsDispatched || m_Event->GetType() != Event::GetStaticType())
            return;
        
        m_IsDispatched = true;
        handler((const Event&)*m_Event);
    }
    template <typename Event, typename Fn>
    requires InputEventDispatchable<Event, Fn>
    void DispatchSource(Fn&& handler)
    {
        if (m_IsDispatched || m_Event->GetSource() != Event::GetStaticSource())
            return;
        
        m_IsDispatched = true;
        handler((const Event&)*m_Event);
    }
private:
    const InputEvent* m_Event{};
    bool m_IsDispatched{false};
};
}
