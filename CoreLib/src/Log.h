#pragma once
#include <filesystem>

namespace lux
{

struct LoggerSettings
{
    std::string LoggerName = "RENDERER";
    std::optional<std::filesystem::path> LogFile;
};

class Logger
{
public:
    static void Init(const LoggerSettings& settings);

    template<typename ...Ts>
    static void Trace(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Trace(std::format(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Info(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Info(std::format(fmt, std::forward<Ts>(args)...));
    }

    template<typename ...Ts>
    static void Warn(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Warn(std::format(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Error(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Error(std::format(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Fatal(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Fatal(std::format(fmt, std::forward<Ts>(args)...));
    }
    
    static void Trace(const std::string& string);
    static void Info(const std::string& string);
    static void Warn(const std::string& string);
    static void Error(const std::string& string);
    static void Fatal(const std::string& string);
};
}

// Macros for engine messages
#define LUX_LOG_TRACE(...)  ::lux::Logger::Trace(__VA_ARGS__)
#define LUX_LOG_INFO(...)   ::lux::Logger::Info(__VA_ARGS__)
#define LUX_LOG_WARN(...)   ::lux::Logger::Warn(__VA_ARGS__)
#define LUX_LOG_ERROR(...)  ::lux::Logger::Error(__VA_ARGS__)
#define LUX_LOG_FATAL(...)  ::lux::Logger::Fatal(__VA_ARGS__)
