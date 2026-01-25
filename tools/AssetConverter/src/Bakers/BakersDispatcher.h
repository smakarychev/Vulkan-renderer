#pragma once
#include "Containers/Span.h"

#include <filesystem>

namespace lux::bakers
{
class BakersDispatcher
{
public:
    BakersDispatcher(const std::filesystem::path& assetPath) : m_AssetPath(assetPath) {}
    template <typename Fn>
    requires requires(Fn dispatch, const std::filesystem::path& path)
    {
        { dispatch(path) } -> std::convertible_to<void>;
    }
    void Dispatch(Span<const std::string_view> extensions, Fn&& dispatchFunction)
    {
        if (m_IsDispatched)
            return;
        
        const auto it = std::ranges::find(extensions, m_AssetPath.extension().string());
        if (it == extensions.end())
            return;
        dispatchFunction(m_AssetPath);
        m_IsDispatched = true;
    }
private:
    bool m_IsDispatched{false};
    std::filesystem::path m_AssetPath;
};
}
