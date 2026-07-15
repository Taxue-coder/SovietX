# ============================================================
# SovietX Windows 安装脚本 (PowerShell)
# ============================================================
#
# 用法：
#   Set-ExecutionPolicy Bypass -Scope Process -Force
#   .\install_windows.ps1
#
# 参数：
#   .\install_windows.ps1 -AutoInject       # 自动查找并注入
#   .\install_windows.ps1 -RestartInject    # 重启微信并注入
#   .\install_windows.ps1 -EnableAutoInject # 开机登录后持续监视并自动注入
#   .\install_windows.ps1 -DisableAutoInject # 停用开机自动注入
#   .\install_windows.ps1 -Uninstall        # 停用 Hook 和托盘 UI
# ============================================================

param(
    [switch]$AutoInject,
    [switch]$RestartInject,
    [switch]$EnableAutoInject,
    [switch]$DisableAutoInject,
    [switch]$Uninstall,
    [int]$TargetPID = 0
)

$ErrorActionPreference = "Stop"

# ============================================================
# 常量
# ============================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$DllPath = Join-Path $ScriptDir "SovietX.dll"
$InjectorPath = Join-Path $ScriptDir "SovietInjector.exe"
$ConfigDir = Join-Path $env:APPDATA "SovietX"
$SupportedWeChatVersion = "4.1.11.54"
$AutoInjectRunKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$AutoInjectValueName = "SovietXAutoInject"
$AutoInjectScriptPath = Join-Path $ScriptDir "auto_inject.vbs"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  SovietX Windows 安装工具" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# 工具函数
# ============================================================

function Write-Ok($msg) {
    Write-Host "[√] " -ForegroundColor Green -NoNewline
    Write-Host $msg
}

function Write-Err($msg) {
    Write-Host "[!] " -ForegroundColor Red -NoNewline
    Write-Host $msg
}

function Write-Info($msg) {
    Write-Host "[*] " -ForegroundColor Yellow -NoNewline
    Write-Host $msg
}

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-WeChatProcesses {
    Get-Process -Name "Weixin" -ErrorAction SilentlyContinue | Where-Object {
        try {
            @($_.Modules | Where-Object { $_.ModuleName -ieq "Weixin.dll" }).Count -gt 0
        } catch {
            $false
        }
    }
}

function Get-WeChatInstallPath {
    # 注册表查找
    $regPaths = @(
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\WeChat",
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\WeChat",
        "HKCU:\SOFTWARE\Tencent\WeChat",
        "HKCU:\SOFTWARE\Tencent\Weixin"
    )

    foreach ($regPath in $regPaths) {
        if (Test-Path $regPath) {
            $installLoc = (Get-ItemProperty $regPath -ErrorAction SilentlyContinue).InstallLocation
            if (-not $installLoc) {
                $installLoc = (Get-ItemProperty $regPath -ErrorAction SilentlyContinue).InstallPath
            }
            if ($installLoc -and (Test-Path (Join-Path $installLoc "Weixin.exe"))) {
                return $installLoc
            }
        }
    }

    # 常见路径
    $commonPaths = @(
        "D:\Program Files\Tencent\Weixin",
        "$env:ProgramFiles\Tencent\Weixin",
        "${env:ProgramFiles(x86)}\Tencent\Weixin",
        "$env:LOCALAPPDATA\Programs\Tencent\Weixin"
    )

    foreach ($path in $commonPaths) {
        if (Test-Path (Join-Path $path "Weixin.exe")) {
            return $path
        }
    }

    return $null
}

function Start-WeChatAndWait {
    $wechatPath = Get-WeChatInstallPath
    if (-not $wechatPath) {
        Write-Err "无法找到微信安装路径，请手动启动微信后重试"
        return $null
    }

    Write-Info "微信路径: $wechatPath"
    Write-Info "正在启动微信..."

    $exePath = Join-Path $wechatPath "Weixin.exe"
    Start-Process $exePath

    # 等待微信进程出现
    Write-Info "等待微信启动..."
    $maxWait = 30
    $proc = $null
    for ($i = 0; $i -lt $maxWait; $i++) {
        Start-Sleep -Seconds 1
        $proc = Get-WeChatProcesses | Select-Object -First 1
        if ($proc) {
            Write-Info "  等待中... $($i + 1)秒" -ForegroundColor DarkYellow
            break
        }
    }

    if (-not $proc) {
        Write-Err "等待超时，微信未成功启动"
        return $null
    }

    # 额外等待初始化
    Write-Info "微信已启动，等待初始化完成..."
    Start-Sleep -Seconds 3

    return $proc
}

function Test-SupportedWeChatProcess([int]$targetPid) {
    $proc = Get-Process -Id $targetPid -ErrorAction SilentlyContinue
    if (-not $proc -or $proc.ProcessName -ne "Weixin") {
        Write-Err "PID $targetPid 不是微信进程"
        return $false
    }

    try {
        $hostModule = @($proc.Modules | Where-Object { $_.ModuleName -ieq "Weixin.dll" })
    } catch {
        Write-Err "无法读取 PID $targetPid 的已加载模块"
        return $false
    }
    if ($hostModule.Count -eq 0) {
        Write-Err "PID $targetPid 不是加载 Weixin.dll 的主进程"
        return $false
    }

    try {
        $version = $proc.MainModule.FileVersion
    } catch {
        Write-Err "无法读取 PID $targetPid 的文件版本"
        return $false
    }

    if ($version -notlike "$SupportedWeChatVersion*") {
        Write-Err "不支持的微信版本: $version（仅允许 $SupportedWeChatVersion）"
        return $false
    }

    return $true
}

function Invoke-Inject([int]$targetPid) {
    if (-not (Test-SupportedWeChatProcess $targetPid)) {
        return $false
    }

    Write-Info "正在注入到 PID $targetPid..."

    $result = & $InjectorPath $targetPid 2>&1
    $result | ForEach-Object { Write-Host $_ }

    if ($LASTEXITCODE -eq 0) {
        Write-Ok "注入成功！"
        return $true
    } else {
        Write-Err "注入失败 (exit code: $LASTEXITCODE)"
        return $false
    }
}

function Invoke-Stop([int]$targetPid) {
    Write-Info "正在停用 PID $targetPid 中的 SovietX..."

    $result = & $InjectorPath --stop $targetPid 2>&1
    $result | ForEach-Object { Write-Host $_ }

    if ($LASTEXITCODE -eq 0) {
        Write-Ok "已停用（DLL 将在微信进程退出后释放）"
        return $true
    }

    Write-Err "停用失败 (exit code: $LASTEXITCODE)"
    return $false
}

function Enable-AutoInjectWatcher {
    if (-not (Test-Path $AutoInjectScriptPath)) {
        Write-Err "未找到自动注入启动器: $AutoInjectScriptPath"
        return $false
    }

    $wscriptPath = Join-Path $env:WINDIR "System32\wscript.exe"
    $runCommand = '"' + $wscriptPath + '" "' + $AutoInjectScriptPath + '"'
    if (-not (Test-Path $AutoInjectRunKey)) {
        New-Item -Path $AutoInjectRunKey | Out-Null
    }
    Set-ItemProperty -Path $AutoInjectRunKey -Name $AutoInjectValueName -Value $runCommand
    Start-Process -FilePath $wscriptPath -ArgumentList @($AutoInjectScriptPath) -WindowStyle Hidden

    Write-Ok "已启用：登录后会自动监视微信并注入"
    return $true
}

function Disable-AutoInjectWatcher {
    Write-Info "正在停用登录后自动注入..."
    Remove-ItemProperty -Path $AutoInjectRunKey -Name $AutoInjectValueName -ErrorAction SilentlyContinue
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "SovietInjector.exe" -and $_.CommandLine -match "--watch" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }

    Write-Ok "已停用登录后自动注入"
    return $true
}

# ============================================================
# 主逻辑
# ============================================================

# 检查管理员权限
if (-not (Test-Admin)) {
    Write-Info "当前以普通用户运行；仅会注入当前用户启动的微信"
} else {
    Write-Ok "管理员权限确认"
}

# 检查必要文件
if (-not (Test-Path $DllPath)) {
    Write-Err "未找到 SovietX.dll"
    Write-Host "  期望路径: $DllPath"
    exit 1
}
Write-Ok "SovietX.dll 已找到"

if (-not (Test-Path $InjectorPath)) {
    Write-Err "未找到 SovietInjector.exe"
    Write-Host "  期望路径: $InjectorPath"
    exit 1
}
Write-Ok "SovietInjector.exe 已找到"

if (-not (Test-Path $AutoInjectScriptPath)) {
    Write-Err "未找到 auto_inject.vbs"
    Write-Host "  期望路径: $AutoInjectScriptPath"
    exit 1
}
Write-Ok "auto_inject.vbs 已找到"
Write-Host ""

# ============================================================
# 处理命令行参数
# ============================================================

if ($DisableAutoInject) {
    $null = Disable-AutoInjectWatcher
    if (-not $Uninstall) { exit 0 }
}

if ($EnableAutoInject) {
    if (Enable-AutoInjectWatcher) { exit 0 }
    exit 1
}

if ($Uninstall) {
    $null = Disable-AutoInjectWatcher
    $procs = if ($TargetPID -gt 0) {
        Get-Process -Id $TargetPID -ErrorAction SilentlyContinue
    } else {
        Get-WeChatProcesses
    }

    if (-not $procs) {
        Write-Info "没有运行中的微信实例，无需停用"
        exit 0
    }

    $failed = $false
    foreach ($proc in $procs) {
        if (-not (Invoke-Stop $proc.Id)) { $failed = $true }
    }
    if ($failed) {
        exit 1
    }
    exit 0
}

if ($AutoInject) {
    $procs = Get-WeChatProcesses
    if (-not $procs) {
        Write-Info "微信未运行，尝试启动..."
        $proc = Start-WeChatAndWait
        if (-not $proc) { exit 1 }
        Invoke-Inject $proc.Id
    } else {
        foreach ($proc in $procs) {
            Invoke-Inject $proc.Id
        }
    }
    exit 0
}

if ($RestartInject) {
    Write-Info "正在关闭微信..."
    Get-WeChatProcesses | Stop-Process -Force
    Start-Sleep -Seconds 2

    $proc = Start-WeChatAndWait
    if (-not $proc) { exit 1 }
    Invoke-Inject $proc.Id
    exit 0
}

if ($TargetPID -gt 0) {
    Invoke-Inject $TargetPID
    exit 0
}

# ============================================================
# 交互式菜单
# ============================================================

Write-Host "请选择操作：" -ForegroundColor White
Write-Host ""

$procs = Get-WeChatProcesses

if ($procs) {
    Write-Host "  当前运行的微信实例：" -ForegroundColor DarkCyan
    foreach ($proc in $procs) {
        Write-Host "    PID: $($proc.Id)  内存: $([math]::Round($proc.WorkingSet64 / 1MB))MB" -ForegroundColor DarkGray
    }
    Write-Host ""

    Write-Host "  1. 注入到第一个微信实例 (PID: $($procs[0].Id))" -ForegroundColor White
    Write-Host "  2. 注入到所有微信实例（多开）" -ForegroundColor White
    Write-Host "  3. 重启微信并注入" -ForegroundColor White
} else {
    Write-Host "  [!] 微信未运行" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  1. 启动微信并注入" -ForegroundColor White
}

Write-Host "  4. 仅显示状态信息" -ForegroundColor White
Write-Host "  5. 停用当前实例中的 SovietX" -ForegroundColor White
Write-Host "  0. 退出" -ForegroundColor White
Write-Host ""

$choice = Read-Host "请输入选项"

switch ($choice) {
    "1" {
        if ($procs) {
            Invoke-Inject $procs[0].Id
        } else {
            $proc = Start-WeChatAndWait
            if ($proc) {
                Invoke-Inject $proc.Id
            }
        }
    }
    "2" {
        if (-not $procs) {
            Write-Err "没有运行中的微信实例"
        } else {
            foreach ($proc in $procs) {
                Invoke-Inject $proc.Id
            }
        }
    }
    "3" {
        Write-Info "正在关闭微信..."
        Get-WeChatProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2

        $proc = Start-WeChatAndWait
        if ($proc) {
            Invoke-Inject $proc.Id
        }
    }
    "4" {
        # 状态信息
        Write-Host ""
        Write-Host "=== 状态信息 ===" -ForegroundColor Cyan

        # DLL 信息
        if (Test-Path $DllPath) {
            $dllInfo = Get-Item $DllPath
            Write-Host "DLL 大小: $([math]::Round($dllInfo.Length / 1KB))KB"
            Write-Host "DLL 修改: $($dllInfo.LastWriteTime)"
        }

        # 微信进程
        $procs = Get-WeChatProcesses
        if ($procs) {
            Write-Host ""
            Write-Host "微信进程：" -ForegroundColor DarkCyan
            foreach ($proc in $procs) {
                $mainModule = $proc.MainModule
                Write-Host "  PID: $($proc.Id)"
                Write-Host "  版本: $($mainModule.FileVersion)"
                Write-Host "  路径: $($mainModule.FileName)"
                Write-Host ""
            }
        } else {
            Write-Host "微信未运行"
        }

        # 配置目录
        if (Test-Path $ConfigDir) {
            Write-Host "配置目录: $ConfigDir" -ForegroundColor DarkCyan
            $configFile = Join-Path $ConfigDir "config.ini"
            if (Test-Path $configFile) {
                Write-Host "配置文件: $configFile ($([math]::Round((Get-Item $configFile).Length))bytes)"
            }
        } else {
            Write-Host "配置目录: 尚未创建"
        }
    }
    "5" {
        if (-not $procs) {
            Write-Err "没有运行中的微信实例"
        } else {
            foreach ($proc in $procs) {
                Invoke-Stop $proc.Id
            }
        }
    }
    default {
        Write-Host "已退出"
    }
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  完成" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
