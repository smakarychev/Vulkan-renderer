#pragma once
#include "types.h"

template <typename T>
class Handle
{
    Handle(u32 id)
        : m_Id(id) {}
    Handle(Handle& other) = default;
    Handle(Handle&& other) = default;
    Handle& operator=(const Handle& other) = default;
    Handle& operator=(Handle&& other) = default;
    ~Handle() = default;

    bool operator==(const Handle& other) const { return m_Id == other.m_Id; }
    bool operator!=(const Handle& other) const { return !(*this == other); }
    
private:
    u32 m_Id;
};
