#pragma once

#include <optional>
#include <string_view>

#include "types.h"
#include "utils/HashedString.h"

class CVarParameter;

enum class CVarFlags : u32
{
    None = 0,
};

class CVars
{
public:
    virtual ~CVars() = default;
    
    static CVars& Get();

    virtual CVarParameter* GetCVar(Utils::HashedString name) = 0;
    
    virtual CVarParameter* CreateF32CVar(Utils::HashedString name, std::string_view description,
        f32 initialVal, f32 val) = 0;
    virtual std::optional<f32> GetF32CVar(Utils::HashedString name) = 0;
    virtual f32 GetF32CVar(Utils::HashedString name, f32 fallback) = 0;
    virtual void SetF32CVar(Utils::HashedString name, f32 value) = 0;
    
    virtual CVarParameter* CreateI32CVar(Utils::HashedString name, std::string_view description,
        i32 initialVal, i32 val) = 0;
    virtual std::optional<i32> GetI32CVar(Utils::HashedString name) = 0;
    virtual i32 GetI32CVar(Utils::HashedString name, i32 fallback) = 0;
    virtual void SetI32CVar(Utils::HashedString name, i32 value) = 0;
    
    virtual CVarParameter* CreateStringCVar(Utils::HashedString name, std::string_view description,
        const std::string& initialVal, const std::string& val) = 0;
    virtual std::optional<std::string> GetStringCVar(Utils::HashedString name) = 0;
    virtual std::string GetStringCVar(Utils::HashedString name, const std::string& fallback) = 0;
    virtual void SetStringCVar(Utils::HashedString name, const std::string& value) = 0;
};

template <typename T>
struct CVar
{
protected:
    u32 m_Index{};
    using CVarType = T;
};

struct CVarF32 : CVar<f32>
{
    CVarF32(Utils::HashedString name, std::string_view description, f32 initialVal, CVarFlags flags = CVarFlags::None);
    f32 Get() const;
    void Set(f32 val) const;
};

struct CVarI32 : CVar<i32>
{
    CVarI32(Utils::HashedString name, std::string_view description, i32 initialVal, CVarFlags flags = CVarFlags::None);
    i32 Get() const;
    void Set(i32 val) const;
};

struct CVarString : CVar<std::string>
{
    CVarString(Utils::HashedString name, std::string_view description, const std::string& initialVal,
        CVarFlags flags = CVarFlags::None);
    std::string Get() const;
    void Set(const std::string& val) const;
};