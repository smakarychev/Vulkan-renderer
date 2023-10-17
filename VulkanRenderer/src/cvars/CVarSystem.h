#pragma once

#include <string_view>

#include "types.h"

class CVarParameter;

enum class CVarFlags : u32
{
    None = 0,
};

class CVarSystem
{
public:
    virtual ~CVarSystem() = default;
    
    static CVarSystem* Get();

    virtual CVarParameter* GetCVar(std::string_view name) = 0;
    virtual CVarParameter* CreateFloatCVar(std::string_view name, std::string_view description, f32 initialVal, f32 val) = 0;
    virtual f32* GetF32CVar(std::string_view name) = 0;
    virtual void SetF32CVar(std::string_view name, f32 value) = 0;
};

template <typename T>
struct CVar
{
protected:
    u32 Index{};
    using CVarType = T;
};

struct CVarFloat : CVar<f32>
{
    CVarFloat(std::string_view name, std::string_view description, f32 initialVal, CVarFlags flags = CVarFlags::None);
    f32 Get();
    void Set(f32 val);
};