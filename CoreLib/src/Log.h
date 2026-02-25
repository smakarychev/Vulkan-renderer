#pragma once
#include "types.h"

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
        Trace(GetLogLine(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Info(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Info(GetLogLine(fmt, std::forward<Ts>(args)...));
    }

    template<typename ...Ts>
    static void Warn(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Warn(GetLogLine(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Error(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Error(GetLogLine(fmt, std::forward<Ts>(args)...));
    }
    
    template<typename ...Ts>
    static void Fatal(std::format_string<Ts...> fmt, Ts&&... args)
    {
        Fatal(GetLogLine(fmt, std::forward<Ts>(args)...));
    }
    
    static void Trace() { Trace(""); }
    static void Info() { Info(""); }
    static void Warn() { Warn(""); }
    static void Error() { Error(""); }
    static void Fatal() { Fatal(""); }
    
    static void Trace(std::string_view string);
    static void Info(std::string_view string);
    static void Warn(std::string_view string);
    static void Error(std::string_view string);
    static void Fatal(std::string_view string);
private:
    template<typename ...Ts>
    static std::string_view GetLogLine(std::format_string<Ts...> fmt, Ts&&... args)
    {
        static constexpr u32 MAX_SIZE = 1024;
        thread_local std::string buffer(MAX_SIZE, 0);
        buffer.clear();

        auto&& [out, _] = std::format_to_n(std::back_inserter(buffer), MAX_SIZE - 1, fmt, std::forward<Ts>(args)...);
        *out = '\0';

        return buffer;
    }
};
}

// Macros for engine messages
#define LUX_LOG_TRACE(...)  ::lux::Logger::Trace(__VA_ARGS__)
#define LUX_LOG_INFO(...)   ::lux::Logger::Info(__VA_ARGS__)
#define LUX_LOG_WARN(...)   ::lux::Logger::Warn(__VA_ARGS__)
#define LUX_LOG_ERROR(...)  ::lux::Logger::Error(__VA_ARGS__)
#define LUX_LOG_FATAL(...)  ::lux::Logger::Fatal(__VA_ARGS__)
