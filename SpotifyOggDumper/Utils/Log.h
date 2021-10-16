#pragma once
#include <format>
#include <iostream>

enum LogLevel
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_LEVEL_COUNT
};

//using macros so we can do this: COL_RED "TUBE"
#define COL_BLACK       "\033[1;30m"
#define COL_DARK_RED    "\033[1;31m"
#define COL_DARK_GREEN  "\033[1;32m"
#define COL_DARK_YELLOW "\033[1;33m"
#define COL_DARK_BLUE   "\033[1;34m"
#define COL_PURPLE      "\033[1;35m"
#define COL_DARK_CYAN   "\033[1;36m"
#define COL_SILVER      "\033[1;37m"
#define COL_GRAY        "\033[1;90m"
#define COL_RED         "\033[1;91m"
#define COL_GREEN       "\033[1;92m"
#define COL_YELLOW      "\033[1;93m"
#define COL_BLUE        "\033[1;94m"
#define COL_PINK        "\033[1;95m"
#define COL_CYAN        "\033[1;96m"
#define COL_WHITE       "\033[1;97m"
#define COL_RESET       "\033[0m"

extern LogLevel LogMinLevel;

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
    constexpr const char* levelColors[] = { COL_GRAY, COL_SILVER, COL_WHITE, COL_YELLOW, COL_RED };
    //constexpr const char* levelNames[] = { "Trace", "Debug", "Info", "Warn", "Error" };

    if (level < LogMinLevel || level >= LOG_LEVEL_COUNT) return;

    std::string str = std::vformat(fmt, std::make_format_args(args...));
    std::cout << levelColors[level] << str << COL_RESET << std::endl;
}

void InstallConsole();