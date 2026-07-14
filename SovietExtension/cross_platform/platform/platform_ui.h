/**
 * @file platform_ui.h
 * @brief SovietExtension 平台 UI 抽象接口
 * 
 * 封装菜单、设置窗口、提示对话框等 UI 操作。
 * - macOS: NSMenu / NSMenuItem / NSAlert
 * - Windows: Win32 CreatePopupMenu / MessageBox / Dialog
 */

#ifndef SOVIET_PLATFORM_UI_H
#define SOVIET_PLATFORM_UI_H

#include <string>
#include <functional>
#include <cstdint>

namespace soviet {

/**
 * 菜单项状态
 */
enum class MenuItemState {
    kOff = 0,
    kOn  = 1,
};

/**
 * 菜单项点击回调
 */
using MenuActionCallback = std::function<void()>;

/**
 * 确认对话框结果
 */
enum class DialogResult {
    kCancel = 0,
    kConfirm = 1,
};

/**
 * 平台 UI 抽象接口。
 */
class PlatformUI {
public:
    virtual ~PlatformUI() = default;

    /**
     * 初始化插件菜单（添加到宿主应用主菜单或系统托盘）。
     */
    virtual void InitPluginMenu() = 0;

    /**
     * 显示确认对话框。
     * @param title 标题
     * @param message 消息内容
     * @param confirmText 确认按钮文本
     * @param cancelText 取消按钮文本
     * @return 用户选择
     */
    virtual DialogResult ShowConfirmDialog(const std::string& title,
                                            const std::string& message,
                                            const std::string& confirmText,
                                            const std::string& cancelText) = 0;

    /**
     * 显示设置窗口（迷离模式等参数配置）。
     */
    virtual void ShowSettingsWindow() = 0;

    /**
     * 刷新菜单项状态（打勾/取消打勾）。
     * @param key 菜单项标识
     * @param state 新状态
     */
    virtual void UpdateMenuItemState(const std::string& key, MenuItemState state) = 0;

    /**
     * 获取菜单项当前状态。
     */
    virtual MenuItemState GetMenuItemState(const std::string& key) = 0;

    /**
     * 判断当前系统外观是否为深色模式。
     */
    virtual bool IsDarkMode() = 0;
};

/**
 * 获取全局 UI 实例（由平台层注入）。
 */
PlatformUI* GetUI();
void SetUI(PlatformUI* ui);

} // namespace soviet

#endif // SOVIET_PLATFORM_UI_H
