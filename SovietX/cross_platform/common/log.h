/**
 * @file log.h
 * @brief SovietX 统一日志系统
 * 
 * 跨平台日志接口。日志同时输出到系统控制台和文件。
 * - macOS: NSLog + /tmp/YMWeChatAntiRevokePatch.log
 * - Windows: OutputDebugString + %TEMP%/YMWeChatAntiRevokePatch.log
 */

#ifndef SOVIET_LOG_H
#define SOVIET_LOG_H

#include <cstdarg>
#include <string>

namespace soviet {

/**
 * 初始化日志系统，设置日志文件路径。
 * 应在插件加载时调用一次。
 * @param logFilePath 日志文件的完整路径
 */
void LogInit(const std::string& logFilePath);

/**
 * 获取当前日志文件路径。
 */
const std::string& LogGetFilePath();

/**
 * 写入一条日志（printf 风格）。
 * 自动加时间戳和前缀 [YMAntiRevoke]。
 */
void Log(const char* format, ...);

/**
 * 写入一条日志（va_list 版本）。
 */
void LogV(const char* format, va_list args);

/**
 * 写入一条带模块标签的日志。
 * @param module 模块名，如 "AntiRevoke"
 */
void LogModule(const char* module, const char* format, ...);

} // namespace soviet

#endif // SOVIET_LOG_H
