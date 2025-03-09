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
    Event() = default;
    Event(const Event&) = default;
    Event& operator=(const Event&) = default;
    Event(Event&& other) noexcept;
    Event& operator=(Event&& other) noexcept;
    ~Event();
    
    void Signal(const Args&... args) const;
    
    void DisconnectHandlers();
    void TransferHandlersTo(Event& other);
private:
    void Connect(Handler& handler) const;
    void Disconnect(Handler& handler) const;

    void PushBackOrTakeFree(Handler& handler) const;
    void RebindHandlers();
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
    EventHandler(const EventHandler& other);
    EventHandler& operator=(const EventHandler& other);
    EventHandler(EventHandler&& other) noexcept;
    EventHandler& operator=(EventHandler&& other) noexcept;
    ~EventHandler();
    
    void Connect(Event<Args...>& event);
    void Disconnect();

    bool IsAttached() const;
private:
    void InheritEventFrom(const EventHandler& other);
private:
    Event<Args...>* m_Event{nullptr};
    Callback m_Callback{};
    i32 m_Index{0};
};


template <typename ... Args>
Event<Args...>::Event(Event&& other) noexcept
    :
    m_Handlers(std::move(other.m_Handlers)),
    m_Pending(std::move(other.m_Pending)),
    m_FreeList(std::move(other.m_FreeList)),
    m_IsSignalling(std::exchange(other.m_IsSignalling, false))
{
    RebindHandlers();
}

template <typename ... Args>
Event<Args...>& Event<Args...>::operator=(Event&& other) noexcept
{
    if (this == &other)
        return *this;

    DisconnectHandlers();
    m_Handlers = std::move(other.m_Handlers);
    m_Pending = std::move(other.m_Pending);
    m_FreeList = std::move(other.m_FreeList);
    m_IsSignalling = std::exchange(other.m_IsSignalling, false);
    RebindHandlers();
    
    return *this;
}

template <typename ... Args>
Event<Args...>::~Event()
{
    DisconnectHandlers();
}

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
void Event<Args...>::DisconnectHandlers()
{
    ASSERT(!m_IsSignalling, "DisconnectHandlers is called during active Signal operation")
    for (auto* handler : m_Handlers)
        if (handler)
            handler->Disconnect();
    for (auto* pending : m_Pending)
        if (pending)
            pending->Disconnect();

    m_Handlers = {};
    m_Pending = {};
    m_FreeList = {};
}

template <typename ... Args>
void Event<Args...>::TransferHandlersTo(Event& other)
{
    auto handlers = std::move(m_Handlers);
    auto pending = std::move(m_Pending);
    m_FreeList = {};
    m_IsSignalling = false;

    auto transfer = [](Handler* handler, Event& event)
    {
        if (!handler)
            return;
        handler->m_Index = 0;
        handler->m_Event = nullptr;
        handler->Connect(event);
    };
    
    for (auto* handler : handlers)
        transfer(handler, other);
    for (auto* handler : pending)
        transfer(handler, other);
}

template <typename ... Args>
void Event<Args...>::Connect(Handler& handler) const
{
    if (m_IsSignalling)
    {
        i32 index = (i32)m_Pending.size();
        handler.m_Index = index;
        m_Pending.push_back(&handler);
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
void Event<Args...>::RebindHandlers()
{
    for (auto* handler : m_Handlers)
        if (handler)
            handler->m_Event = this;
    for (auto* pending : m_Pending)
        if (pending)
            pending->m_Event = this;
}


template <typename ... Args>
EventHandler<Args...>::EventHandler(Callback callback)
    : m_Callback(std::move(callback))
{
}

template <typename ... Args>
EventHandler<Args...>::EventHandler(const EventHandler& other)
    : m_Event(other.m_Event), m_Callback(other.m_Callback), m_Index(other.m_Index)
{
    if (m_Event && m_Callback)
        m_Event->Connect(*this);
    else
        m_Event = nullptr;
}

template <typename ... Args>
EventHandler<Args...>& EventHandler<Args...>::operator=(const EventHandler& other)
{
    if (this == &other)
        return *this;

    Disconnect();
    m_Event = other.m_Event;
    m_Callback = other.m_Callback;
    m_Index = other.m_Index;

    if (m_Event && m_Callback)
        m_Event->Connect(*this);
    else
        m_Event = nullptr;
    
    return *this;
}

template <typename ... Args>
EventHandler<Args...>::EventHandler(EventHandler&& other) noexcept
    :
    m_Event(other.m_Event),
    m_Callback(std::exchange(other.m_Callback, nullptr)),
    m_Index(std::exchange(other.m_Index, 0))
{
    InheritEventFrom(other);   
}

template <typename ... Args>
EventHandler<Args...>& EventHandler<Args...>::operator=(EventHandler&& other) noexcept
{
    if (this == &other)
        return *this;

    Disconnect();
    m_Event = std::exchange(other.m_Event, nullptr);
    m_Callback = std::move(other.m_Callback);
    m_Index = std::exchange(other.m_Index, 0);
    InheritEventFrom(other);

    return *this;
}

template <typename ... Args>
EventHandler<Args...>::~EventHandler()
{
    Disconnect();
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

template <typename ... Args>
bool EventHandler<Args...>::IsAttached() const
{
    return m_Event != nullptr;
}

template <typename ... Args>
void EventHandler<Args...>::InheritEventFrom(const EventHandler& other)
{
    if (!m_Event)
        return;

    if (m_Index < 0)
    {
        ASSERT(m_Event->m_Pending[-m_Index + 1] == &other, "EventHandler mismatch")
        m_Event->m_Pending[-m_Index + 1] = this;
    }
    else
    {
        ASSERT(m_Event->m_Handlers[m_Index] == &other, "EventHandler mismatch")
        m_Event->m_Handlers[m_Index] = this;
    }
}
