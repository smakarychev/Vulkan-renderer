#pragma once

#include "ViewInfoGPU.h"
#include "String/StringId.h"

class SceneViewInfo
{
public:
    using HandleType = u64;
    SceneViewInfo() = default;
    template <typename T>
    SceneViewInfo(T handle, VisibilityFlags visibilityFlags) : m_VisibilityFlags(visibilityFlags)
    {
        static_assert(sizeof(T) <= sizeof(HandleType));
        HandleType handleType = {};
        std::memcpy(&handleType, &handle, sizeof handle);
        m_View = handleType;
    }
    SceneViewInfo(const ViewInfoGPU& viewInfo) :
        m_View(viewInfo), m_VisibilityFlags((::VisibilityFlags)viewInfo.Camera.VisibilityFlags) {}

    ViewInfoGPU AsViewInfoGPU() const { return std::get<ViewInfoGPU>(m_View); }
    template <typename T>
    T AsHandle() const
    {
        static_assert(sizeof(T) <= sizeof(HandleType));
        T handle = {};
        std::memcpy(&handle, &std::get<HandleType>(m_View), sizeof handle);
        
        return handle;
    }

    bool IsViewInfoGPU() const { return std::holds_alternative<ViewInfoGPU>(m_View); }
    bool IsHandle() const { return std::holds_alternative<HandleType>(m_View); }

    VisibilityFlags VisibilityFlags() const { return m_VisibilityFlags; }

private:
    std::variant<HandleType, ViewInfoGPU> m_View{ViewInfoGPU{}};
    ::VisibilityFlags m_VisibilityFlags{VisibilityFlags::None};
};

struct SceneView
{
    StringId Name{};
    SceneViewInfo ViewInfo{};
    auto operator==(const SceneView& other) const { return Name == other.Name; }
};