@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

title SovietExtension Installer for Windows

echo ============================================================
echo   SovietExtension Windows 安装脚本
echo ============================================================
echo.

:: ============================================================
:: 检查管理员权限
:: ============================================================
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [错误] 需要以管理员身份运行此脚本！
    echo 请右键点击此文件，选择"以管理员身份运行"
    echo.
    pause
    exit /b 1
)

echo [√] 管理员权限确认
echo.

:: ============================================================
:: 检查必要文件
:: ============================================================
set "SCRIPT_DIR=%~dp0"
set "DLL_PATH=%SCRIPT_DIR%SovietExtension.dll"
set "INJECTOR_PATH=%SCRIPT_DIR%SovietInjector.exe"

if not exist "%DLL_PATH%" (
    echo [错误] 未找到 SovietExtension.dll
    echo 请确保 DLL 文件与此脚本在同一目录
    echo 路径: %DLL_PATH%
    pause
    exit /b 1
)

if not exist "%INJECTOR_PATH%" (
    echo [错误] 未找到 SovietInjector.exe
    echo 请确保注入器与此脚本在同一目录
    pause
    exit /b 1
)

echo [√] SovietExtension.dll 已找到
echo [√] SovietInjector.exe 已找到
echo.

:: ============================================================
:: 查找微信进程
:: ============================================================
echo [*] 正在查找微信进程...

set "WECHAT_PID="
for /f "tokens=2" %%i in ('tasklist /fi "IMAGENAME eq WeChat.exe" /fo list 2^>nul ^| findstr "PID:"') do (
    set "WECHAT_PID=%%i"
    goto :found_pid
)

echo [!] 未找到运行中的微信进程
echo.
echo 请选择操作：
echo   1. 启动微信并等待注入
echo   2. 退出
echo.
set /p choice="请输入选项 (1/2): "

if "%choice%"=="1" goto :launch_wechat
goto :exit

:found_pid
echo [√] 找到微信进程 PID: %WECHAT_PID%
echo.

:: ============================================================
:: 功能选择菜单
:: ============================================================
echo 请选择操作：
echo   1. 注入到当前微信实例 (PID: %WECHAT_PID%)
echo   2. 注入到所有微信实例（多开）
echo   3. 关闭微信 → 重新启动 → 自动注入
echo   4. 退出
echo.
set /p action="请输入选项 (1-4): "

if "%action%"=="1" goto :inject_single
if "%action%"=="2" goto :inject_all
if "%action%"=="3" goto :restart_inject
if "%action%"=="4" goto :exit

echo [!] 无效选项
goto :exit

:: ============================================================
:: 注入到单个实例
:: ============================================================
:inject_single
echo.
echo [*] 正在注入到 PID %WECHAT_PID%...
"%INJECTOR_PATH%" %WECHAT_PID%
if %errorLevel% equ 0 (
    echo.
    echo [√] 注入成功！
) else (
    echo.
    echo [!] 注入失败，请检查错误信息
)
echo.
pause
goto :exit

:: ============================================================
:: 注入到所有实例
:: ============================================================
:inject_all
echo.
echo [*] 正在注入到所有微信实例...
"%INJECTOR_PATH%" --multi
if %errorLevel% equ 0 (
    echo.
    echo [√] 全部注入成功！
) else (
    echo.
    echo [!] 部分或全部注入失败
)
echo.
pause
goto :exit

:: ============================================================
:: 重启并注入
:: ============================================================
:restart_inject
echo.
echo [*] 正在关闭微信...
taskkill /f /im WeChat.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [*] 正在重新启动微信...
goto :launch_wechat

:: ============================================================
:: 启动微信并注入
:: ============================================================
:launch_wechat
:: 尝试从注册表获取微信安装路径
set "WECHAT_PATH="

for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\WeChat" /v InstallLocation 2^>nul') do (
    set "WECHAT_PATH=%%b"
)

if not defined WECHAT_PATH (
    for /f "tokens=2*" %%a in ('reg query "HKCU\SOFTWARE\Tencent\WeChat" /v InstallPath 2^>nul') do (
        set "WECHAT_PATH=%%b"
    )
)

if not defined WECHAT_PATH (
    :: 常见安装路径
    if exist "%ProgramFiles%\Tencent\WeChat\WeChat.exe" (
        set "WECHAT_PATH=%ProgramFiles%\Tencent\WeChat"
    ) else if exist "%ProgramFiles(x86)%\Tencent\WeChat\WeChat.exe" (
        set "WECHAT_PATH=%ProgramFiles(x86)%\Tencent\WeChat"
    ) else if exist "%LOCALAPPDATA%\Programs\Tencent\WeChat\WeChat.exe" (
        set "WECHAT_PATH=%LOCALAPPDATA%\Programs\Tencent\WeChat"
    )
)

if not defined WECHAT_PATH (
    echo [!] 无法找到微信安装路径
    echo 请手动启动微信后重新运行此脚本
    pause
    goto :exit
)

echo [*] 微信路径: %WECHAT_PATH%
echo [*] 正在启动微信...
start "" "%WECHAT_PATH%\WeChat.exe"

echo [*] 等待微信启动...
set /a wait_count=0

:wait_loop
timeout /t 1 /nobreak >nul
set /a wait_count+=1

tasklist /fi "IMAGENAME eq WeChat.exe" 2>nul | findstr "WeChat" >nul
if %errorLevel% neq 0 (
    if %wait_count% lss 30 (
        echo     等待中... %wait_count%秒
        goto :wait_loop
    )
    echo [!] 等待超时，微信可能未成功启动
    pause
    goto :exit
)

:: 额外等待，让微信完成初始化
echo [*] 微信已启动，等待初始化完成...
timeout /t 3 /nobreak >nul

:: 获取新的 PID
for /f "tokens=2" %%i in ('tasklist /fi "IMAGENAME eq WeChat.exe" /fo list 2^>nul ^| findstr "PID:"') do (
    set "WECHAT_PID=%%i"
    goto :do_inject
)

:do_inject
echo [√] 微信 PID: %WECHAT_PID%
echo [*] 正在注入...
"%INJECTOR_PATH%" %WECHAT_PID%
if %errorLevel% equ 0 (
    echo.
    echo [√] 注入成功！微信插件已加载
) else (
    echo.
    echo [!] 注入失败
)
echo.
pause
goto :exit

:exit
echo.
echo ============================================================
echo   安装完成
echo ============================================================
endlocal
