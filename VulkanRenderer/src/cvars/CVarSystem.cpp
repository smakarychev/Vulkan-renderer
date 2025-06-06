﻿#include "CVarSystem.h"

#include "types.h"

#include <array>
#include <string>
#include <unordered_map>


enum class CVarType : u8
{
    None,
    Int,
    Float,
    String
};

struct CVarParameter
{
    u32 ArrayIndex;
    CVarType Type;
    CVarFlags Flags;
    std::string Name;
    std::string Description;
};

template <typename T>
struct CVarRecord
{
    T InitialValue;
    T Value;

    CVarParameter* Parameter;
};

template <typename T>
struct CVarArrayTraits
{
    static constexpr u32 SIZE = 0;
    static constexpr auto TYPE = CVarType::None;
};

template <>
struct CVarArrayTraits<i32>
{
    static constexpr u32 SIZE = 256;
    static constexpr auto TYPE = CVarType::Int;
};

template <>
struct CVarArrayTraits<f32>
{
    static constexpr u32 SIZE = 256;
    static constexpr auto TYPE = CVarType::Float;
};

template <>
struct CVarArrayTraits<std::string>
{
    static constexpr u32 SIZE = 256;
    static constexpr auto TYPE = CVarType::String;
};

template <typename T>
struct CVarArray
{
    const T& GetValue(u32 index) const;
    T* GetValuePtr(u32 index);
    void SetValue(const T& val, u32 index);
    u32 AddValue(const T& initialVal, const T& val, CVarParameter* parameter);

public:
    std::array<CVarRecord<T>, CVarArrayTraits<T>::SIZE> Storage;
    u32 CurrentCVar{0};
};

template <typename T>
const T& CVarArray<T>::GetValue(u32 index) const
{
    return Storage[index].Value;
}

template <typename T>
T* CVarArray<T>::GetValuePtr(u32 index)
{
    return &Storage[index].Value;
}

template <typename T>
void CVarArray<T>::SetValue(const T& val, u32 index)
{
    Storage[index].Value = val;
}

template <typename T>
u32 CVarArray<T>::AddValue(const T& initialVal, const T& val, CVarParameter* parameter)
{
    u32 index = CurrentCVar;
    Storage[index].InitialValue = initialVal;
    Storage[index].Value = val;
    Storage[index].Parameter = parameter;
    parameter->ArrayIndex = index;
    parameter->Type = CVarArrayTraits<T>::TYPE;

    CurrentCVar++;

    return index;
}

class CVarsImpl : public CVars
{
public:
    CVarArray<i32> IntCVars{};
    CVarArray<f32> FloatCVars{};
    CVarArray<std::string> StringCVars{};

    template <typename T>
    CVarArray<T>& GetCVarArray();


    CVarParameter* CreateF32CVar(StringId name, std::string_view description,
        f32 initialVal, f32 val) final;
    std::optional<f32> GetF32CVar(StringId name) final;
    f32 GetF32CVar(StringId name, f32 fallback) final;
    void SetF32CVar(StringId name, f32 value) final;

    CVarParameter* CreateI32CVar(StringId name, std::string_view description,
        i32 initialVal, i32 val) override;
    std::optional<i32> GetI32CVar(StringId name) final;
    i32 GetI32CVar(StringId name, i32 fallback) final;
    void SetI32CVar(StringId name, i32 value) final;

    CVarParameter* CreateStringCVar(StringId name, std::string_view description,
        const std::string& initialVal, const std::string& val) final;
    std::optional<std::string> GetStringCVar(StringId name) final;
    std::string GetStringCVar(StringId name, const std::string& fallback) final;
    void SetStringCVar(StringId name, const std::string& value) final;

private:
    CVarParameter* GetCVar(StringId name);
    CVarParameter* InitCVar(StringId name, std::string_view description);
    template <typename T>
    CVarParameter* CreateCVar(StringId name, std::string_view description,
        const T& initialVal, const T& val);

    template <typename T>
    std::optional<T> GetCVarValue(StringId name);

    template <typename T>
    void SetCVarValue(StringId name, const T& val);
private:
    std::unordered_map<StringId, CVarParameter> m_CVars;
};

template <>
CVarArray<i32>& CVarsImpl::GetCVarArray() { return IntCVars; }

template <>
CVarArray<f32>& CVarsImpl::GetCVarArray() { return FloatCVars; }

template <>
CVarArray<std::string>& CVarsImpl::GetCVarArray() { return StringCVars; }

CVarParameter* CVarsImpl::GetCVar(StringId name)
{
    if (m_CVars.contains(name))
        return &m_CVars.at(name);

    return nullptr;
}

CVarParameter* CVarsImpl::CreateF32CVar(StringId name, std::string_view description,
    f32 initialVal, f32 val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<f32> CVarsImpl::GetF32CVar(StringId name)
{
    return GetCVarValue<f32>(name);
}

f32 CVarsImpl::GetF32CVar(StringId name, f32 fallback)
{
    auto var = GetF32CVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetF32CVar(StringId name, f32 value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::CreateI32CVar(StringId name, std::string_view description,
    i32 initialVal, i32 val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<i32> CVarsImpl::GetI32CVar(StringId name)
{
    return GetCVarValue<i32>(name);
}

i32 CVarsImpl::GetI32CVar(StringId name, i32 fallback)
{
    auto var = GetI32CVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetI32CVar(StringId name, i32 value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::CreateStringCVar(StringId name, std::string_view description,
    const std::string& initialVal, const std::string& val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<std::string> CVarsImpl::GetStringCVar(StringId name)
{
    return GetCVarValue<std::string>(name);
}

std::string CVarsImpl::GetStringCVar(StringId name, const std::string& fallback)
{
    auto var = GetStringCVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetStringCVar(StringId name, const std::string& value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::InitCVar(StringId name, std::string_view description)
{
    if (GetCVar(name))
        return nullptr;

    CVarParameter newCVar = {};
    newCVar.Name = std::string{name.AsString()};
    newCVar.Description = description;
    m_CVars.emplace(std::make_pair(name, newCVar));

    return &m_CVars.at(name);
}

template <typename T>
CVarParameter* CVarsImpl::CreateCVar(StringId name, std::string_view description,
    const T& initialVal, const T& val)
{
    CVarParameter* parameter = InitCVar(name, description);
    if (parameter == nullptr)
        return nullptr;

    GetCVarArray<T>().AddValue(initialVal, val, parameter);

    return parameter;
}

CVars& CVars::Get()
{
    static CVarsImpl system = {};

    return system;
}

template <typename T>
std::optional<T> CVarsImpl::GetCVarValue(StringId name)
{
    CVarParameter* parameter = GetCVar(name);

    return parameter ? std::optional(GetCVarArray<T>().GetValue(parameter->ArrayIndex)) : std::nullopt;
}

template <typename T>
void CVarsImpl::SetCVarValue(StringId name, const T& val)
{
    CVarParameter* parameter = GetCVar(name);
    if (parameter != nullptr)
        GetCVarArray<T>().SetValue(val, parameter->ArrayIndex);
}


namespace
{
    template <typename T>
    T getCVarValueByIndex(u32 index)
    {
        return ((CVarsImpl&)CVarsImpl::Get()).GetCVarArray<T>().GetValue(index);
    }

    template <typename T>
    void setCVarValueByIndex(u32 index, const T& val)
    {
        ((CVarsImpl&)CVarsImpl::Get()).GetCVarArray<T>().SetValue(val, index);
    }
}

CVarF32::CVarF32(StringId name, std::string_view description, f32 initialVal, CVarFlags flags)
{
    CVarParameter* cvar = CVars::Get().CreateF32CVar(name, description, initialVal, initialVal);
    cvar->Flags = flags;
    m_Index = cvar->ArrayIndex;
}

f32 CVarF32::Get() const
{
    return getCVarValueByIndex<CVarType>(m_Index);
}

void CVarF32::Set(f32 val) const
{
    setCVarValueByIndex<CVarType>(m_Index, val);
}

CVarI32::CVarI32(StringId name, std::string_view description, i32 initialVal, CVarFlags flags)
{
    CVarParameter* cvar = CVars::Get().CreateI32CVar(name, description, initialVal, initialVal);
    cvar->Flags = flags;
    m_Index = cvar->ArrayIndex;
}

i32 CVarI32::Get() const
{
    return getCVarValueByIndex<CVarType>(m_Index);
}

void CVarI32::Set(i32 val) const
{
    setCVarValueByIndex<CVarType>(m_Index, val);
}

CVarString::CVarString(StringId name, std::string_view description, const std::string& initialVal,
    CVarFlags flags)
{
    CVarParameter* cvar = CVars::Get().CreateStringCVar(name, description, initialVal, initialVal);
    cvar->Flags = flags;
    m_Index = cvar->ArrayIndex;
}

std::string CVarString::Get() const
{
    return getCVarValueByIndex<CVarType>(m_Index);
}

void CVarString::Set(const std::string& val) const
{
    setCVarValueByIndex<CVarType>(m_Index, val);
}
