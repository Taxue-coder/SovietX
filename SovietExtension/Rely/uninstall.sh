#!/bin/bash

# 如果用户用 sh uninstall.sh 执行，自动切换到 bash
# If user runs this script with sh, re-exec with bash.
if [ -z "${BASH_VERSION:-}" ]; then
    exec /bin/bash "$0" "$@"
fi

set -euo pipefail

# ==============================
# SovietX uninstaller
# ==============================

APP_NAME="WeChat"
FRAMEWORK_NAME="${FRAMEWORK_NAME:-SovietX}"
APP_PATH="/Applications/${APP_NAME}.app"
FORCE=0
REMOVE_BACKUP=0
RUN_SUDO=0

die() {
    echo ""
    echo "❌ [ERROR] $*" >&2
    echo ""
    exit 1
}

warn() {
    echo "⚠️  [WARN] $*"
}

ok() {
    echo "✅ [OK] $*"
}

info() {
    echo "👉 [INFO] $*"
}

usage() {
    cat <<EOF
Usage:
  ./uninstall.sh
  sh uninstall.sh
  ./uninstall.sh --remove-backup
  ./uninstall.sh --force
  ./uninstall.sh --app=/Applications/WeChat.app

Options:
  --force              Allow restoring from non-current backup / 允许使用非当前版本备份恢复
  --remove-backup      Remove backup files after uninstall / 卸载后删除备份
  --app=PATH           Specify WeChat.app path / 指定 WeChat.app 路径
  --framework=NAME     Specify framework name, default: SovietX / 指定插件名，默认 SovietX
  -h, --help           Show help / 显示帮助

EOF
}

run_cmd() {
    if [ "${RUN_SUDO}" -eq 1 ]; then
        sudo "$@"
    else
        "$@"
    fi
}

for arg in "$@"; do
    case "$arg" in
        --force)
            FORCE=1
            ;;
        --remove-backup)
            REMOVE_BACKUP=1
            ;;
        --app=*)
            APP_PATH="${arg#--app=}"
            ;;
        --framework=*)
            FRAMEWORK_NAME="${arg#--framework=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown argument / 未知参数: ${arg}"
            ;;
    esac
done

APP_PATH="${APP_PATH%/}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MACOS_PATH="${APP_PATH}/Contents/MacOS"
INFO_PLIST="${APP_PATH}/Contents/Info.plist"
APP_EXECUTABLE_PATH="${MACOS_PATH}/${APP_NAME}"

FRAMEWORK_DST_PATH="${MACOS_PATH}/${FRAMEWORK_NAME}.framework"
STATE_FILE="${MACOS_PATH}/.${FRAMEWORK_NAME}.install_state"

read_plist() {
    local key="$1"
    /usr/libexec/PlistBuddy -c "Print :${key}" "${INFO_PLIST}" 2>/dev/null || true
}

read_state_value() {
    local key="$1"

    if [ ! -f "${STATE_FILE}" ]; then
        return 0
    fi

    grep "^${key}=" "${STATE_FILE}" 2>/dev/null | head -n 1 | sed "s/^${key}=//" || true
}

check_basic_files() {
    [ -d "${APP_PATH}" ] || die "WeChat.app not found / 找不到 WeChat.app: ${APP_PATH}"
    [ -f "${INFO_PLIST}" ] || die "Info.plist not found / 找不到 Info.plist: ${INFO_PLIST}"
    [ -f "${APP_EXECUTABLE_PATH}" ] || die "WeChat executable not found / 找不到微信主可执行文件: ${APP_EXECUTABLE_PATH}"
}

prepare_version_and_backup() {
    APP_SHORT_VERSION="$(read_plist CFBundleShortVersionString)"
    APP_BUILD_VERSION="$(read_plist CFBundleVersion)"

    [ -n "${APP_SHORT_VERSION}" ] || die "Failed to read CFBundleShortVersionString / 读取微信版本号失败"
    [ -n "${APP_BUILD_VERSION}" ] || die "Failed to read CFBundleVersion / 读取微信 build 号失败"

    echo ""
    info "Detected WeChat version / 检测到微信版本:"
    echo "    CFBundleShortVersionString: ${APP_SHORT_VERSION}"
    echo "    CFBundleVersion:            ${APP_BUILD_VERSION}"
    echo ""

    BACKUP_PATH_FROM_STATE="$(read_state_value backup)"
    if [ -n "${BACKUP_PATH_FROM_STATE}" ]; then
        BACKUP_PATH="${BACKUP_PATH_FROM_STATE}"
        info "Backup path from install state / 从安装状态读取备份路径:"
        echo "    ${BACKUP_PATH}"
    else
        BACKUP_PATH="${APP_EXECUTABLE_PATH}.backup.${APP_SHORT_VERSION}.${APP_BUILD_VERSION}"
        info "Backup path by current version / 按当前版本推导备份路径:"
        echo "    ${BACKUP_PATH}"
    fi

    if [ -f "${BACKUP_PATH}" ]; then
        ok "Backup found / 找到备份"
        return 0
    fi

    local candidate=""
    candidate="$(ls -t "${APP_EXECUTABLE_PATH}.backup."* 2>/dev/null | head -n 1 || true)"

    if [ -n "${candidate}" ]; then
        warn "Exact backup not found, but another backup exists / 未找到精确备份，但找到了其他备份:"
        echo "    ${candidate}"

        if [ "${FORCE}" -eq 1 ]; then
            warn "Force mode enabled, use this backup / 已使用 --force，将使用该备份恢复"
            BACKUP_PATH="${candidate}"
            return 0
        fi

        read -r -p "Use this backup to restore? 是否使用这个备份恢复？[y/N] " answer
        case "${answer}" in
            y|Y|yes|YES)
                BACKUP_PATH="${candidate}"
                ;;
            *)
                warn "User refused non-current backup / 用户拒绝使用非当前版本备份"
                ;;
        esac
    fi
}

prepare_sudo() {
    RUN_SUDO=0

    if [ ! -w "${MACOS_PATH}" ] || [ ! -w "${APP_EXECUTABLE_PATH}" ]; then
        RUN_SUDO=1
        info "Administrator permission required / 需要管理员权限，准备申请 sudo..."
        sudo -v
    fi
}

quit_wechat() {
    info "Quit WeChat / 退出微信..."

    osascript -e 'tell application "WeChat" to quit' >/dev/null 2>&1 || true
    sleep 1

    pkill -x WeChat >/dev/null 2>&1 || true

    for _ in 1 2 3 4 5 6 7 8 9 10; do
        if ! pgrep -x WeChat >/dev/null 2>&1; then
            ok "WeChat is not running / 微信已退出"
            return 0
        fi
        sleep 0.5
    done

    if pgrep -x WeChat >/dev/null 2>&1; then
        warn "WeChat is still running, force kill / 微信仍在运行，强制结束"
        pkill -9 -x WeChat >/dev/null 2>&1 || true
    fi
}

is_injected() {
    if otool -l "${APP_EXECUTABLE_PATH}" 2>/dev/null | grep -q "${FRAMEWORK_NAME}"; then
        return 0
    fi

    return 1
}

restore_executable() {
    info "Restore original executable / 恢复微信主可执行文件..."

    if [ -f "${BACKUP_PATH}" ]; then
        run_cmd cp -p "${BACKUP_PATH}" "${APP_EXECUTABLE_PATH}"
        run_cmd chmod +x "${APP_EXECUTABLE_PATH}"
        ok "Executable restored from backup / 已从备份恢复: ${BACKUP_PATH}"
        return 0
    fi

    if is_injected; then
        die "Injected LC_LOAD_DYLIB still exists, but backup not found / 当前主程序仍含有插件注入项，但找不到备份，无法安全卸载"
    fi

    ok "No injected dylib found, skip restore / 未检测到插件注入项，跳过恢复"
}

remove_framework() {
    info "Remove plugin framework / 删除插件 framework..."

    if [ -d "${FRAMEWORK_DST_PATH}" ]; then
        run_cmd rm -rf "${FRAMEWORK_DST_PATH}"
        ok "Framework removed / 已删除: ${FRAMEWORK_DST_PATH}"
    else
        ok "Framework does not exist, skip / 插件 framework 不存在，跳过"
    fi

    if [ -f "${STATE_FILE}" ]; then
        run_cmd rm -f "${STATE_FILE}"
        ok "Install state removed / 安装状态已删除"
    fi
}

sign_app() {
    info "Code sign WeChatAppEx if exists / 如果存在则重新签名 WeChatAppEx..."
    APP_EX_PATH="${MACOS_PATH}/WeChatAppEx.app"

    if [ -d "${APP_EX_PATH}" ]; then
        run_cmd codesign --force --deep --sign - --timestamp=none "${APP_EX_PATH}" || true

        WEAPP_PATH="${APP_EX_PATH}/Contents/Frameworks/WeChatAppEx Framework.framework/Versions/C/Helpers/WeApp.app"
        if [ -d "${WEAPP_PATH}" ]; then
            run_cmd codesign --force --deep --sign - --timestamp=none "${WEAPP_PATH}" || true
        fi
    fi

    info "Code sign main WeChat.app / 重新签名主 WeChat.app..."
    run_cmd codesign --force --deep --sign - --timestamp=none "${APP_PATH}"

    ok "Code sign finished / 签名完成"
}

remove_backup_if_needed() {
    if [ "${REMOVE_BACKUP}" -ne 1 ]; then
        ok "Backup kept / 已保留备份文件"
        return 0
    fi

    info "Remove backup files / 删除备份文件..."
    run_cmd rm -f "${APP_EXECUTABLE_PATH}.backup."*
    ok "Backup removed / 备份已删除"
}

verify_uninstall() {
    info "Verify uninstall / 检查卸载结果..."

    if is_injected; then
        die "Injected LC_LOAD_DYLIB still exists / 卸载后仍然检测到 ${FRAMEWORK_NAME}"
    fi

    ok "No injected LC_LOAD_DYLIB found / 已确认主程序中没有 ${FRAMEWORK_NAME}"

    echo ""
    info "Verify code signature / 检查签名..."

    if codesign -vvv --deep --strict "${APP_PATH}" >/dev/null 2>&1; then
        ok "Code signature verified / 签名验证通过"
    else
        warn "Code signature verification failed, but app may still run for debugging / 签名验证未完全通过，但调试运行不一定受影响"
    fi
}

print_done() {
    echo ""
    echo "=============================="
    echo "✅ ${FRAMEWORK_NAME} uninstalled successfully"
    echo "✅ ${FRAMEWORK_NAME} 卸载完成"
    echo "=============================="
    echo ""
    echo "Run WeChat / 启动微信："
    echo "  open -a WeChat"
    echo ""
}

echo "=============================="
echo " Uninstall ${FRAMEWORK_NAME}"
echo "=============================="
echo "APP_PATH=${APP_PATH}"
echo "FRAMEWORK_DST_PATH=${FRAMEWORK_DST_PATH}"
echo ""

check_basic_files
prepare_version_and_backup
prepare_sudo
quit_wechat
restore_executable
remove_framework
sign_app
remove_backup_if_needed
verify_uninstall
print_done