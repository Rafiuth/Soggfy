#pragma once
#include <format>
#include <iostream>
#include <filesystem>

enum LogLevel
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_LEVEL_COUNT
};
extern LogLevel LogMinLevel;

void InitLogger(bool printToConsole, const std::filesystem::path& logFile);
void CloseLogger();
void _WriteLog(LogLevel level, const std::string& ln);

//Exclude comma with zero args (https://stackoverflow.com/a/5897216)
#define VA_ARGS(...) , ##__VA_ARGS__
#define LogTrace(fmt, ...) WriteLog(LOG_TRACE, fmt VA_ARGS(__VA_ARGS__))
#define LogDebug(fmt, ...) WriteLog(LOG_DEBUG, fmt VA_ARGS(__VA_ARGS__))
#define LogInfo(fmt, ...)  WriteLog(LOG_INFO,  fmt VA_ARGS(__VA_ARGS__))
#define LogWarn(fmt, ...)  WriteLog(LOG_WARN,  fmt VA_ARGS(__VA_ARGS__))
#define LogError(fmt, ...) WriteLog(LOG_ERROR, fmt VA_ARGS(__VA_ARGS__))

template <typename... Args>
void WriteLog(LogLevel level, std::string_view fmt, Args&&... args)
{
    if (level >= LogMinLevel && level < LOG_LEVEL_COUNT) {
        _WriteLog(level, std::vformat(fmt, std::make_format_args(args...)));
    }
}