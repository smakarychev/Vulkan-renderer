#pragma once

#include "core.h"
#include "types.h"

#include <functional>

template <typename ... Args>
class SignalHandler;

template <typename ... Args>
class Signal
{
    using Handler = SignalHandler<Args...>;
    friend class Handler;
public:
    Signal() = default;
    Signal(const Signal&) = default;
    Signal& operator=(const Signal&) = default;
    Signal(Signal&& other) noexcept;
    Signal& operator=(Signal&& other) noexcept;
    ~Signal();
    
    void Emit(const Args&... args) const;
    
    void DisconnectHandlers();
    void TransferHandlersTo(Signal& other);
private:
    void Connect(Handler& handler) const;
    void Disconnect(Handler& handler) const;

    void PushBackOrTakeFree(Handler& handler) const;
    void RebindHandlers();
private:
    mutable std::vector<Handler*> m_Handlers;
    /* handlers that were added while executing Emit, from other handler's callback */
    mutable std::vector<Handler*> m_Pending;
    mutable std::vector<u32> m_FreeList;
    mutable bool m_IsEmitting{false};
};


template <typename ... Args>
class SignalHandler
{
    friend class Signal<Args...>;
public:
    using Callback = std::function<void(Args...)>;

    SignalHandler() = default;
    SignalHandler(Callback callback);
    SignalHandler(const SignalHandler& other);
    SignalHandler& operator=(const SignalHandler& other);
    SignalHandler(SignalHandler&& other) noexcept;
    SignalHandler& operator=(SignalHandler&& other) noexcept;
    ~SignalHandler();
    
    void Connect(Signal<Args...>& signal);
    void Disconnect();

    bool IsAttached() const;
private:
    void InheritSignalFrom(const SignalHandler& other);
private:
    Signal<Args...>* m_Signal{nullptr};
    Callback m_Callback{};
    i32 m_Index{0};
};


template <typename ... Args>
Signal<Args...>::Signal(Signal&& other) noexcept :
    m_Handlers(std::move(other.m_Handlers)),
    m_Pending(std::move(other.m_Pending)),
    m_FreeList(std::move(other.m_FreeList)),
    m_IsEmitting(std::exchange(other.m_IsEmitting, false))
{
    RebindHandlers();
}

template <typename ... Args>
Signal<Args...>& Signal<Args...>::operator=(Signal&& other) noexcept
{
    if (this == &other)
        return *this;

    DisconnectHandlers();
    m_Handlers = std::move(other.m_Handlers);
    m_Pending = std::move(other.m_Pending);
    m_FreeList = std::move(other.m_FreeList);
    m_IsEmitting = std::exchange(other.m_IsEmitting, false);
    RebindHandlers();
    
    return *this;
}

template <typename ... Args>
Signal<Args...>::~Signal()
{
    DisconnectHandlers();
}

template <typename ... Args>
void Signal<Args...>::Emit(const Args&... args) const
{
    m_IsEmitting = true;
    for (auto* handler : m_Handlers)
        if (handler)
            handler->m_Callback(args...);

    for (auto* pending : m_Pending)
        PushBackOrTakeFree(*pending);
    m_Pending.clear();
            
    m_IsEmitting = false;
}

template <typename ... Args>
void Signal<Args...>::DisconnectHandlers()
{
    ASSERT(!m_IsEmitting, "DisconnectHandlers is called during active Emit operation")
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
void Signal<Args...>::TransferHandlersTo(Signal& other)
{
    auto handlers = std::move(m_Handlers);
    auto pending = std::move(m_Pending);
    m_FreeList = {};
    m_IsEmitting = false;

    auto transfer = [](Handler* handler, Signal& signal)
    {
        if (!handler)
            return;
        handler->m_Index = 0;
        handler->m_Signal = nullptr;
        handler->Connect(signal);
    };
    
    for (auto* handler : handlers)
        transfer(handler, other);
    for (auto* handler : pending)
        transfer(handler, other);
}

template <typename ... Args>
void Signal<Args...>::Connect(Handler& handler) const
{
    if (m_IsEmitting)
    {
        i32 index = (i32)m_Pending.size();
        handler.m_Index = index;
        m_Pending.push_back(&handler);
        return;
    }

    PushBackOrTakeFree(handler);
}

template <typename ... Args>
void Signal<Args...>::Disconnect(Handler& handler) const
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
void Signal<Args...>::PushBackOrTakeFree(Handler& handler) const
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
void Signal<Args...>::RebindHandlers()
{
    for (auto* handler : m_Handlers)
        if (handler)
            handler->m_Signal = this;
    for (auto* pending : m_Pending)
        if (pending)
            pending->m_Signal = this;
}


template <typename ... Args>
SignalHandler<Args...>::SignalHandler(Callback callback) :
    m_Callback(std::move(callback))
{
}

template <typename ... Args>
SignalHandler<Args...>::SignalHandler(const SignalHandler& other) :
    m_Signal(other.m_Signal),
    m_Callback(other.m_Callback),
    m_Index(other.m_Index)
{
    if (m_Signal && m_Callback)
        m_Signal->Connect(*this);
    else
        m_Signal = nullptr;
}

template <typename ... Args>
SignalHandler<Args...>& SignalHandler<Args...>::operator=(const SignalHandler& other)
{
    if (this == &other)
        return *this;

    Disconnect();
    m_Signal = other.m_Signal;
    m_Callback = other.m_Callback;
    m_Index = other.m_Index;

    if (m_Signal && m_Callback)
        m_Signal->Connect(*this);
    else
        m_Signal = nullptr;
    
    return *this;
}

template <typename ... Args>
SignalHandler<Args...>::SignalHandler(SignalHandler&& other) noexcept :
    m_Signal(other.m_Signal),
    m_Callback(std::exchange(other.m_Callback, nullptr)),
    m_Index(std::exchange(other.m_Index, 0))
{
    InheritSignalFrom(other);   
}

template <typename ... Args>
SignalHandler<Args...>& SignalHandler<Args...>::operator=(SignalHandler&& other) noexcept
{
    if (this == &other)
        return *this;

    Disconnect();
    m_Signal = std::exchange(other.m_Signal, nullptr);
    m_Callback = std::move(other.m_Callback);
    m_Index = std::exchange(other.m_Index, 0);
    InheritSignalFrom(other);

    return *this;
}

template <typename ... Args>
SignalHandler<Args...>::~SignalHandler()
{
    Disconnect();
}

template <typename ... Args>
void SignalHandler<Args...>::Connect(Signal<Args...>& signal)
{
    ASSERT(m_Callback, "Handler callback does not exist")
    ASSERT(!m_Signal, "Handler has already been attached to a signal")
    m_Signal = &signal;
    signal.Connect(*this);
}

template <typename ... Args>
void SignalHandler<Args...>::Disconnect()
{
    if (m_Signal)
    {
        m_Signal->Disconnect(*this);
        m_Signal = nullptr;
    }
}

template <typename ... Args>
bool SignalHandler<Args...>::IsAttached() const
{
    return m_Signal != nullptr;
}

template <typename ... Args>
void SignalHandler<Args...>::InheritSignalFrom(const SignalHandler& other)
{
    if (!m_Signal)
        return;

    if (m_Index < 0)
    {
        ASSERT(m_Signal->m_Pending[-m_Index + 1] == &other, "SignalHandler mismatch")
        m_Signal->m_Pending[-m_Index + 1] = this;
    }
    else
    {
        ASSERT(m_Signal->m_Handlers[m_Index] == &other, "SignalHandler mismatch")
        m_Signal->m_Handlers[m_Index] = this;
    }
}
