#pragma once

#include <filesystem>

class ConverterDispatcher
{
public:
    ConverterDispatcher(const std::filesystem::path& file)
        : m_File(&file) {}

    template <typename Conv>
    void Dispatch(const std::vector<std::string_view>& extensions)
    {
        if (m_IsDispatched)
            return;
        auto it = std::ranges::find(extensions, m_File->extension().string());
        if (it != extensions.end())
        {
            m_IsDispatched = true;
            if (Conv::NeedsConversion(*m_File))
                Conv::Convert(*m_File);
        }
    }
    
private:
    const std::filesystem::path* m_File{nullptr};
    bool m_IsDispatched{false};
};