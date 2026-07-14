/**
 * @file log.cpp
 * @brief SovietExtension 统一日志系统实现
 */

#include "log.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace soviet {

static std::string g_logFilePath;
static std::mutex g_logMutex;

void LogInit(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logFilePath = logFilePath;
}

const std::string& LogGetFilePath() {
    return g_logFilePath;
}

static std::string FormatTimestamp() {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return std::string(buf);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             static_cast<int>(tv.tv_usec / 1000));
    return std::string(buf);
#endif
}

static void WriteToFile(const std::string& line) {
    if (g_logFilePath.empty()) {
        return;
    }

    FILE* fp = fopen(g_logFilePath.c_str(), "a");
    if (fp) {
        fputs(line.c_str(), fp);
        fputc('\n', fp);
        fclose(fp);
    }
}

static void WriteToConsole(const std::string& line) {
#ifdef _WIN32
    // Windows: OutputDebugString for debugger, stderr for console
    OutputDebugStringA((line + "\n").c_str());
    fprintf(stderr, "%s\n", line.c_str());
#else
    // macOS/Linux: fprintf to stderr
    fprintf(stderr, "%s\n", line.c_str());
#endif
}

void LogV(const char* format, va_list args) {
    char msgBuf[4096];
    vsnprintf(msgBuf, sizeof(msgBuf), format, args);

    std::string timestamp = FormatTimestamp();
    char lineBuf[4352];
    snprintf(lineBuf, sizeof(lineBuf), "[%s] [YMAntiRevoke] %s", timestamp.c_str(), msgBuf);

    std::string line(lineBuf);

    std::lock_guard<std::mutex> lock(g_logMutex);
    WriteToConsole(line);
    WriteToFile(line);
}

void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LogV(format, args);
    va_end(args);
}

void LogModule(const char* module, const char* format, ...) {
    char msgBuf[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(msgBuf, sizeof(msgBuf), format, args);
    va_end(args);

    std::string timestamp = FormatTimestamp();
    char lineBuf[4352];
    snprintf(lineBuf, sizeof(lineBuf), "[%s] [YMAntiRevoke] [%s] %s",
             timestamp.c_str(), module ? module : "?", msgBuf);

    std::string line(lineBuf);

    std::lock_guard<std::mutex> lock(g_logMutex);
    WriteToConsole(line);
    WriteToFile(line);
}

} // namespace soviet
