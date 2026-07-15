/**
 * @file platform_process.h
 * @brief SovietX 平台进程管理抽象接口
 * 
 * 封装进程启动、终止、互斥体等进程级操作。
 * - macOS: NSTask / killall / open -n
 * - Windows: CreateProcess / TerminateProcess / CreateMutex
 */

#ifndef SOVIET_PLATFORM_PROCESS_H
#define SOVIET_PLATFORM_PROCESS_H

#include <string>
#include <cstdint>

namespace soviet {

/**
 * 平台进程管理抽象接口。
 */
class PlatformProcess {
public:
    virtual ~PlatformProcess() = default;

    /**
     * 重启宿主应用（微信）。
     * 先退出再重新启动。
     */
    virtual void RestartHostApp() = 0;

    /**
     * 退出宿主应用。
     */
    virtual void QuitHostApp() = 0;

    /**
     * 启动新的宿主应用实例（多开）。
     */
    virtual bool LaunchNewInstance() = 0;

    /**
     * 执行 shell/命令行命令。
     * @param command 命令字符串
     * @return 命令输出
     */
    virtual std::string ExecuteCommand(const std::string& command) = 0;

    /**
     * 获取宿主应用路径。
     * - macOS: /Applications/WeChat.app
     * - Windows: D:\Program Files\Tencent\Weixin\Weixin.exe
     */
    virtual std::string GetHostAppPath() = 0;

    /**
     * 获取宿主应用版本号。
     */
    virtual std::string GetHostAppVersion() = 0;

    /**
     * 获取宿主应用 Build 号。
     */
    virtual std::string GetHostAppBuildVersion() = 0;

    /**
     * 用系统默认浏览器打开 URL。
     */
    virtual bool OpenURLInSystemBrowser(const std::string& url) = 0;
};

/**
 * 获取全局进程管理实例（由平台层注入）。
 */
PlatformProcess* GetProcess();
void SetProcess(PlatformProcess* process);

} // namespace soviet

#endif // SOVIET_PLATFORM_PROCESS_H
