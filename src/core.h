#pragma once

#include <format>
#include <iostream>

class Logger
{
public:
    template <typename ... Types>
    static void Log(const std::_Fmt_string<Types...> formatString, Types&&... args)
    {
        std::cout << std::format(formatString, std::forward<Types>(args)...);
        std::cout << "\n";
    }
    static void Log(const std::string& message)
    {
        std::cout << message << "\n";
    }
};

#define LOG(...) Logger::Log(__VA_ARGS__)

#define ASSERT(x, ...) if(x) {} else { LOG("Assertion failed"); LOG(__VA_ARGS__); __debugbreak(); }

inline void VulkanCheck(VkResult res, std::string_view message)
{
    if (res != VK_SUCCESS)
    {
        LOG(message.data());
        abort();
    }
}
