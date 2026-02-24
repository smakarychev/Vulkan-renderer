#include "Log.h"

#include <stacktrace>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace 
{
std::unique_ptr<spdlog::logger> g_Logger;
}

namespace lux
{
void Logger::Init(const LoggerSettings& settings)
{
    std::vector<spdlog::sink_ptr> sinks;

    if (settings.LogFile.has_value())
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            settings.LogFile->string(), /*truncate*/true));
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    g_Logger = std::make_unique<spdlog::logger>(settings.LoggerName, begin(sinks), end(sinks));
    g_Logger->set_pattern("%^%Y-%m-%d %T [%L:%t] %n: %v%$");
    g_Logger->set_level(spdlog::level::trace);
}

void Logger::Trace(const std::string& string)
{
    g_Logger->trace(string);
}

void Logger::Info(const std::string& string)
{
    g_Logger->info(string);
}

void Logger::Warn(const std::string& string)
{
    g_Logger->warn(string);
}

void Logger::Error(const std::string& string)
{
    g_Logger->error(string);
}

void Logger::Fatal(const std::string& string)
{
    g_Logger->error("FATAL ERROR OCCURRED");
    g_Logger->error(string);
    g_Logger->error("{}", to_string(std::stacktrace::current()));
    g_Logger->flush();
    __debugbreak();
}
}
