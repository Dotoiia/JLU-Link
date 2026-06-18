# JLU Link

一款为吉林大学校园网（jlu）设计的 macOS 认证客户端。基于 `drcom-jlu-qt` 重构，面向 Apple Silicon，重点改善原项目在现代 macOS 上的兼容性、界面体验和菜单栏驻留方式。

> 非吉林大学官方软件。使用前请确认你有权接入所在网络，并遵守学校网络管理规定。

![JLU Link 界面](docs/screenshot.png)

## 特色

- **原生 Apple Silicon**：面向 arm64 构建，不依赖 Rosetta 2。
- **现代 macOS 界面**：重新设计登录、状态和偏好区域，支持浅色/深色外观；在支持的系统上使用原生 Liquid Glass 材质。
- **真正的菜单栏模式**：关闭主窗口后继续后台认证，并从 Dock 隐藏；可从菜单栏重新打开、连接、断开、重启或退出。
- **清晰的连接反馈**：在线状态显示绿色指示，断线状态和提示文案更明确。
- **简化 MAC 配置**：只保留一个可编辑、可记忆的 MAC 地址；首次使用时自动探测有效地址。
- **更友好的登录体验**：支持密码显隐、登录信息保存、启动自动连接和断线自动重连。
- **更安全的日志**：日志写入用户数据目录，不再修改应用包；认证报文中的账号相关内容不会写入日志。
- **macOS 风格图标**：独立 Dock 图标与单色菜单栏状态图标，适配浅色和深色菜单栏。

## 下载与安装

在仓库的 [Releases](https://github.com/Dotoiia/JLU-Link/releases) 页面下载最新 DMG：

1. 打开 DMG。
2. 将 `JLU Link.app` 拖入“应用程序”。
3. 首次启动若被 macOS 拦截，请在“系统设置 → 隐私与安全性”中选择“仍要打开”。

当前发布包使用 ad-hoc 签名，尚未使用 Apple Developer ID 公证。

## 相比上游的主要修改

本项目源自 [code4lala/drcom-jlu-qt](https://github.com/code4lala/drcom-jlu-qt)，并参考其 macOS 构建方式。主要修改包括：

- 迁移并验证 Qt 6 / Apple Silicon arm64 构建。
- 修复下载隔离、资源签名和应用包内写日志导致的启动与签名问题。
- 将应用名称、Bundle Identifier 和图标统一为 JLU Link。
- 全面重做 macOS 界面、中文文案、在线/离线状态和密码显隐交互。
- 将复杂网卡选择简化为单一、可记忆的 MAC 地址。
- 增加断线自动重连，并理顺“保存信息 / 自动连接 / 自动重连”之间的依赖关系。
- 重新设计菜单栏图标和菜单；关闭窗口后保持认证并隐藏 Dock 图标。
- 增加原生最小化、窗口淡入和状态过渡，并适配浅色/深色模式。
- 在 macOS 26 使用 `NSGlassEffectView`，旧系统回退到兼容材质。
- 将“关于 JLU Link”链接改为本仓库。

更细的来源与修改记录见 [NOTICE.md](NOTICE.md)。

## 从源码构建

需要 macOS、Xcode Command Line Tools 和 Qt 6（Core、Gui、Widgets、Network）。

```bash
qmake DrCOM_JLU_Qt.pro CONFIG+=release
make -j$(sysctl -n hw.logicalcpu)
macdeployqt DrCOM_JLU_Qt.app
codesign --force --deep --sign - DrCOM_JLU_Qt.app
```

项目也保留了上游的 Windows/Linux 代码路径，但本仓库的重点和已验证发布目标是 macOS arm64。

## 数据与隐私

账号、密码、MAC 地址和偏好由 Qt `QSettings` 存储在本机。软件不会把这些信息上传到本仓库或第三方分析服务。校园网认证流量仍会按协议发送给吉林大学认证服务器。

## 致谢

- 上游项目：[code4lala/drcom-jlu-qt](https://github.com/code4lala/drcom-jlu-qt)
- 协议资料：[drcoms/jlu-drcom-client](https://github.com/drcoms/jlu-drcom-client)
- 单实例组件：[itay-grudev/SingleApplication](https://github.com/itay-grudev/SingleApplication)
- dogcom 相关实现：[mchome/dogcom](https://github.com/mchome/dogcom)

## 许可证

本项目遵循上游的 [GNU Affero General Public License v3.0](LICENSE)。修改版继续在相同许可下发布。
