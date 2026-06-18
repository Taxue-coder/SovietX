<p align="center">
<img src="https://github.com/MustangYM/SovietExtension/blob/main/3.png" width="600px"/>
</p>

# SovietExtension苏维埃助手
For开源共产主义，for理想主义，免费的，抽象的，令人愉快的 A Plugin For Mac WeChat > 4.0
## Effect
<p align="center">
<img src="https://github.com/MustangYM/SovietExtension/blob/main/1.1.png" width="600px"/>
</p>
<p align="center">
<img src="https://github.com/MustangYM/SovietExtension/blob/main/2.png" width="600px"/>
</p>

## Supported Version
**睁大眼睛看**，目前**只支持**以下M芯片版本， 本人没有Intel机器无法开发，所以其余版无效,随缘适配，QT化之后逆向起来很麻烦，代码已完全开源，可自行查看，爱你。
由于[微信官网](https://mac.weixin.qq.com/)大版本看起来一样，但小版本可能不一致，注意仔细核对下面的版本！
- [4.1.10.53(268853)](https://weixin.qq.com/updates?platform=mac&version=4.1.10) M芯片，不支持Intel，截止2026-6-19我在官网下载到的
- [4.1.9.58(268602)](https://github.com/zsbai/wechat-versions/releases/tag/4.1.9.58) M芯片，不支持Intel
## Install
1.如果是刚安装的微信，**请先手动打开一次微信后**，再安装插件，否则安装完插件会提示"xxx已损坏"！

2.Rely文件夹 - > install.sh执行这个安装脚本即可，效果如下
```
mustangym@macdeMacBook-Pro Rely % sh /Users/mustangym/SovietExtension/SovietExtension/Rely/install.sh 
==============================
 Install SovietExtension
==============================
APP_PATH=/Applications/WeChat.app
PLUGIN_SRC_PATH=/Users/mustangym/SovietExtension/SovietExtension/Rely/Plugin/SovietExtension.framework
FRAMEWORK_DST_PATH=/Applications/WeChat.app/Contents/MacOS/SovietExtension.framework
INSERT_DYLIB_PATH=/Users/mustangym/SovietExtension/SovietExtension/Rely/insert_dylib
SUPPORTED_FILE=/Users/mustangym/SovietExtension/SovietExtension/Rely/supported_versions.txt
LOAD_DYLIB_PATH=@executable_path/SovietExtension.framework/SovietExtension


👉 [INFO] Detected WeChat version / 检测到微信版本:
    CFBundleShortVersionString: 4.1.9
    CFBundleVersion:            268602

✅ [OK] Version supported / 版本检查通过
    Supported Display Version: 4.1.9.58
    Matched Rule:              4.1.9.58|4.1.9|268602|Tested on Mac WeChat 4.1.9.58

....省略一万句

👉 [INFO] Verify code signature / 检查签名...
⚠️  [WARN] Code signature verification failed, but app may still run for debugging / 签名验证未完全通过，但调试运行不一定受影响

==============================
✅ SovietExtension installed successfully
✅ SovietExtension 安装完成
==============================

Run WeChat and watch log / 启动微信并查看日志：
  rm -f /tmp/YMWeChatAntiRevokePatch.log
  open -a WeChat
  tail -f /tmp/YMWeChatAntiRevokePatch.log

Uninstall / 卸载：
  /Users/mustangym/SovietExtension/SovietExtension/Rely/uninstall.sh
```
如果报错：`cp: xxxxx:Operation not permitted`，
```
请到 系统设置 → 隐私与安全性，给当前你运行脚本这个"终端"工具开启：
1. 完整磁盘访问权限 / Full Disk Access
2. App 管理 / App Management
```
## Uninstall
同理执行uninstall.sh即可

## Thanks
不接受除Bug以外的任何issue，不接受任何形式捐赠与收费，湖畔大学全体同学，**瑞思拜**，MustangYM.

## License
<a href="LICENSE"><img src="https://img.shields.io/github/license/fstudio/clangbuilder.svg"></a>
<a href="https://996.icu"><img src="https://img.shields.io/badge/link-996.icu-red.svg"></a>
