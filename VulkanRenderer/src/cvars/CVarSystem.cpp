#include "CVarSystem.h"

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

class CVarParameter
{
public:
    friend class CVarsImpl;

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

    CVarParameter* GetCVar(Utils::HashedString name) final;

    CVarParameter* CreateFloatCVar(Utils::HashedString name, std::string_view description,
        f32 initialVal, f32 val) final;
    std::optional<f32> GetF32CVar(Utils::HashedString name) final;
    f32 GetF32CVar(Utils::HashedString name, f32 fallback) override;
    void SetF32CVar(Utils::HashedString name, f32 value) final;

    CVarParameter* CreateIntCVar(Utils::HashedString name, std::string_view description,
        i32 initialVal, i32 val) override;
    std::optional<i32> GetI32CVar(Utils::HashedString name) override;
    i32 GetI32CVar(Utils::HashedString name, i32 fallback) override;
    void SetI32CVar(Utils::HashedString name, i32 value) override;

    CVarParameter* CreateStringCVar(Utils::HashedString name, std::string_view description,
        const std::string& initialVal, const std::string& val) override;
    std::optional<std::string> GetStringCVar(Utils::HashedString name) override;
    std::string GetStringCVar(Utils::HashedString name, const std::string& fallback) override;
    void SetStringCVar(Utils::HashedString name, const std::string& value) override;

private:
    CVarParameter* InitCVar(Utils::HashedString name, std::string_view description);
    template <typename T>
    CVarParameter* CreateCVar(Utils::HashedString name, std::string_view description,
        const T& initialVal, const T& val);

    template <typename T>
    std::optional<T> GetCVarValue(Utils::HashedString name);

    template <typename T>
    void SetCVarValue(Utils::HashedString name, const T& val);
private:
    std::unordered_map<u64, CVarParameter> m_CVars;
};

template <>
CVarArray<i32>& CVarsImpl::GetCVarArray() { return IntCVars; }

template <>
CVarArray<f32>& CVarsImpl::GetCVarArray() { return FloatCVars; }

template <>
CVarArray<std::string>& CVarsImpl::GetCVarArray() { return StringCVars; }

CVarParameter* CVarsImpl::GetCVar(Utils::HashedString name)
{
    if (m_CVars.contains(name.Hash()))
        return &m_CVars.at(name.Hash());

    return nullptr;
}

CVarParameter* CVarsImpl::CreateFloatCVar(Utils::HashedString name, std::string_view description,
    f32 initialVal, f32 val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<f32> CVarsImpl::GetF32CVar(Utils::HashedString name)
{
    return GetCVarValue<f32>(name);
}

f32 CVarsImpl::GetF32CVar(Utils::HashedString name, f32 fallback)
{
    auto var = GetF32CVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetF32CVar(Utils::HashedString name, f32 value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::CreateIntCVar(Utils::HashedString name, std::string_view description, i32 initialVal,
    i32 val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<i32> CVarsImpl::GetI32CVar(Utils::HashedString name)
{
    return GetCVarValue<i32>(name);
}

i32 CVarsImpl::GetI32CVar(Utils::HashedString name, i32 fallback)
{
    auto var = GetI32CVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetI32CVar(Utils::HashedString name, i32 value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::CreateStringCVar(Utils::HashedString name, std::string_view description,
    const std::string& initialVal, const std::string& val)
{
    return CreateCVar(name, description, initialVal, val);
}

std::optional<std::string> CVarsImpl::GetStringCVar(Utils::HashedString name)
{
    return GetCVarValue<std::string>(name);
}

std::string CVarsImpl::GetStringCVar(Utils::HashedString name, const std::string& fallback)
{
    auto var = GetStringCVar(name);
    
    return var.has_value() ? *var : fallback;
}

void CVarsImpl::SetStringCVar(Utils::HashedString name, const std::string& value)
{
    SetCVarValue(name, value);
}

CVarParameter* CVarsImpl::InitCVar(Utils::HashedString name, std::string_view description)
{
    if (GetCVar(name))
        return nullptr;

    CVarParameter newCVar = {};
    newCVar.Name = std::string{name.String()};
    newCVar.Description = description;
    m_CVars.emplace(std::make_pair(name.Hash(), newCVar));

    return &m_CVars.at(name.Hash());
}

template <typename T>
CVarParameter* CVarsImpl::CreateCVar(Utils::HashedString name, std::string_view description,
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
std::optional<T> CVarsImpl::GetCVarValue(Utils::HashedString name)
{
    CVarParameter* parameter = GetCVar(name);

    return parameter ? std::optional(GetCVarArray<T>().GetValue(parameter->ArrayIndex)) : std::nullopt;
}

template <typename T>
void CVarsImpl::SetCVarValue(Utils::HashedString name, const T& val)
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

CVarF32::CVarF32(Utils::HashedString name, std::string_view description, f32 initialVal, CVarFlags flags)
{
    CVarParameter* cvar = CVars::Get().CreateFloatCVar(name, description, initialVal, initialVal);
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

CVarI32::CVarI32(Utils::HashedString name, std::string_view description, i32 initialVal, CVarFlags flags)
{
    CVarParameter* cvar = CVars::Get().CreateIntCVar(name, description, initialVal, initialVal);
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

CVarString::CVarString(Utils::HashedString name, std::string_view description, const std::string& initialVal,
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
