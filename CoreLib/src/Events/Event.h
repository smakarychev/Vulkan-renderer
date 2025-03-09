#pragma once

#include "core.h"
#include "types.h"

#include <functional>

template <typename ... Args>
class EventHandler;

template <typename ... Args>
class Event
{
    using Handler = EventHandler<Args...>;
    friend class Handler;
public:
    void Signal(const Args&... args) const;
private:
    void Connect(Handler& handler) const;
    void Disconnect(Handler& handler) const;

    void PushBackOrTakeFree(Handler& handler) const;
private:
    mutable std::vector<Handler*> m_Handlers;
    /* handlers that were added while executing Signal, from other handler's callback */
    mutable std::vector<Handler*> m_Pending;
    mutable std::vector<u32> m_FreeList;
    mutable bool m_IsSignalling{false};
};


template <typename ... Args>
class EventHandler
{
    friend class Event<Args...>;
public:
    using Callback = std::function<void(Args...)>;

    EventHandler() = default;
    EventHandler(Callback callback);
    
    void Connect(Event<Args...>& event);
    void Disconnect();
private:
    Event<Args...>* m_Event{nullptr};
    Callback m_Callback{};
    i32 m_Index{0};
};



template <typename ... Args>
void Event<Args...>::Signal(const Args&... args) const
{
    m_IsSignalling = true;
    for (auto* handler : m_Handlers)
        if (handler)
            handler->m_Callback(args...);

    for (auto* pending : m_Pending)
        PushBackOrTakeFree(*pending);
    m_Pending.clear();
            
    m_IsSignalling = false;
}

template <typename ... Args>
void Event<Args...>::Connect(Handler& handler) const
{
    if (m_IsSignalling)
    {
        i32 index = (i32)m_Pending.size();
        handler.m_Index = index;
        return;
    }

    PushBackOrTakeFree(handler);
}

template <typename ... Args>
void Event<Args...>::Disconnect(Handler& handler) const
{
    const i32 index = handler.m_Index;
    if (index < 0)
    {
        m_Pending[-index + 1] = nullptr;
    }
    else
    {
        m_Handlers[index] = nullptr;
        m_FreeList.push_back((u32)index);        
    }
}

template <typename ... Args>
void Event<Args...>::PushBackOrTakeFree(Handler& handler) const
{
    if (m_FreeList.empty())
    {
        const i32 index = (i32)m_Handlers.size();
        m_Handlers.emplace_back(&handler);
        handler.m_Index = index;
    }
    else
    {
        const i32 index = m_FreeList.back();
        m_FreeList.pop_back();
        ASSERT(m_Handlers[index] == nullptr, "Handler index collision")
        m_Handlers[index] = &handler;
        handler.m_Index = index;
    }
}


template <typename ... Args>
EventHandler<Args...>::EventHandler(Callback callback)
    : m_Callback(std::move(callback))
{
}

template <typename ... Args>
void EventHandler<Args...>::Connect(Event<Args...>& event)
{
    ASSERT(m_Callback, "Handler callback does not exist")
    ASSERT(!m_Event, "Handler has already been attached to an event")
    m_Event = &event;
    event.Connect(*this);
}

template <typename ... Args>
void EventHandler<Args...>::Disconnect()
{
    if (m_Event)
    {
        m_Event->Disconnect(*this);
        m_Event = nullptr;
    }
}
