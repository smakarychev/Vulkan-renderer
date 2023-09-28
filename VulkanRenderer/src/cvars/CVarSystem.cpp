#include "CVarSystem.h"

#include "types.h"

#include <string>
#include <unordered_map>

enum class CVarType : u8
{
    Int,
    Float,
    String
};

class CVarParameter
{
public: 
    friend class CVarSystemImpl;

    u32 ArrayIndex;
    CVarType Type;
    CVarFlags Flags;
    std::string Name;
    std::string Description;
};

template <typename T>
struct CVarStorage
{
    T InitialValue;
    T Value;

    CVarParameter* Parameter;
};

template <typename T>
struct CVarArray
{
    CVarArray(u32 sizeElements);
    ~CVarArray();

    const T& GetValue(u32 index) const;
    T* GetValuePtr(u32 index);
    void SetValue(const T& val, u32 index);
    u32 AddValue(const T& initialVal, const T& val, CVarParameter* parameter);
public:
    CVarStorage<T>* Storage{nullptr};
    u32 CurrentCVar{0};
};

template <typename T>
CVarArray<T>::CVarArray(u32 sizeElements)
{
    Storage = new CVarStorage<T>[sizeElements]();
}

template <typename T>
CVarArray<T>::~CVarArray()
{
    delete[] Storage;
}

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

    CurrentCVar++;
    return index;
}

class CVarSystemImpl : public CVarSystem
{
public:
    static constexpr u32 MAX_INT_CVARS = 256; 
    static constexpr u32 MAX_FLOAT_CVARS = 256; 
    static constexpr u32 MAX_STRING_CVARS = 256;

    CVarArray<i32> IntCVars{MAX_INT_CVARS};
    CVarArray<f32> FloatCVars{MAX_FLOAT_CVARS};
    CVarArray<std::string> StringCVars{MAX_STRING_CVARS};

    template <typename T>
    CVarArray<T>& GetCVarArray();

    template<>
    CVarArray<i32>& GetCVarArray() { return IntCVars; }
    template<>
    CVarArray<f32>& GetCVarArray() { return FloatCVars; }
    template<>
    CVarArray<std::string>& GetCVarArray() { return StringCVars; }

    CVarParameter* GetCVar(utils::StringHash stringHash) final;
    CVarParameter* CreateFloatCVar(std::string_view name, std::string_view description, f32 initialVal, f32 val) final;
    f32* GetFloatCVar(utils::StringHash stringHash) final;
    void SetFloatCVar(utils::StringHash stringHash, f32 value) final;
private:
    CVarParameter* InitCVar(std::string_view name, std::string_view description);

    template <typename T>
    T* GetCVarValue(utils::StringHash stringHash);

    template <typename T>
    void SetCVarValue(utils::StringHash stringHash, const T& val);
private:
    std::unordered_map<u64, CVarParameter> m_CVars; 
};



CVarParameter* CVarSystemImpl::GetCVar(utils::StringHash stringHash)
{
    if (m_CVars.contains(stringHash))
        return &m_CVars.at(stringHash);
    return nullptr;
}

CVarParameter* CVarSystemImpl::CreateFloatCVar(std::string_view name, std::string_view description, f32 initialVal, f32 val)
{
    CVarParameter* parameter = InitCVar(name, description);
    if (parameter == nullptr)
        return nullptr;

    parameter->Type = CVarType::Float;
    GetCVarArray<f32>().AddValue(initialVal, val, parameter);

    return parameter;
}

f32* CVarSystemImpl::GetFloatCVar(utils::StringHash stringHash)
{
    return GetCVarValue<f32>(stringHash);
}

void CVarSystemImpl::SetFloatCVar(utils::StringHash stringHash, f32 value)
{
    SetCVarValue(stringHash, value);
}

CVarParameter* CVarSystemImpl::InitCVar(std::string_view name, std::string_view description)
{
    if (GetCVar(name))
        return nullptr;

    CVarParameter newCVar = {};
    newCVar.Name = name;
    newCVar.Description = description;
    utils::StringHash namehash{name};
    m_CVars.emplace(std::make_pair(namehash, newCVar));
    
    return &m_CVars.at(namehash);
}

CVarSystem* CVarSystem::Get()
{
    static CVarSystemImpl system = {};
    return &system;
}

template <typename T>
T* CVarSystemImpl::GetCVarValue(utils::StringHash stringHash)
{
    CVarParameter* parameter = GetCVar(stringHash);
    return parameter ? GetCVarArray<T>().GetValuePtr(parameter->ArrayIndex) : nullptr; 
}

template <typename T>
void CVarSystemImpl::SetCVarValue(utils::StringHash stringHash, const T& val)
{
    CVarParameter* parameter = GetCVar(stringHash);
    if (parameter != nullptr)
        GetCVarArray<T>().SetValue(val, parameter->ArrayIndex);
}


namespace
{
    template <typename T>
    T getCVarValueByIndex(u32 index)
    {
        return ((CVarSystemImpl*)CVarSystemImpl::Get())->GetCVarArray<T>().GetValue(index);
    }

    template <typename T>
    void setCVarValueByIndex(u32 index, const T& val)
    {
        ((CVarSystemImpl*)CVarSystemImpl::Get())->GetCVarArray<T>().SetValue(val, index);
    }
}

CVarFloat::CVarFloat(std::string_view name, std::string_view description, f32 initialVal, CVarFlags flags)
{
    CVarParameter* cvar = CVarSystem::Get()->CreateFloatCVar(name, description, initialVal, initialVal);
    cvar->Flags = flags;
    Index = cvar->ArrayIndex;
}

f32 CVarFloat::Get()
{
    return getCVarValueByIndex<CVarType>(Index);
}

void CVarFloat::Set(f32 val)
{
    setCVarValueByIndex<CVarType>(Index, val);
}
