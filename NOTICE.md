# 来源与修改说明

JLU Link 是 `drcom-jlu-qt` 的修改版，而非吉林大学官方客户端。

## 上游

- 原仓库：https://github.com/code4lala/drcom-jlu-qt
- 上游许可证：GNU Affero General Public License v3.0
- 本次采用的上游基线：`50d09b8`（`master`）

## 本仓库的修改

本修改版针对 macOS 和 Apple Silicon 重新构建，修改集中在：

1. Qt 6 与 arm64 编译兼容。
2. macOS 应用包、Bundle 信息、图标、签名和日志路径。
3. 登录窗口、状态显示、偏好分组、密码显隐和 MAC 地址输入。
4. 自动连接与断线自动重连。
5. 菜单栏驻留、Dock 显示策略、托盘状态图标和菜单操作。
6. 浅色/深色主题，以及支持系统上的原生 Liquid Glass 材质。
7. 更简洁的中文状态和断线提示。
8. “关于 JLU Link”指向本仓库。

原作者和贡献者的版权声明、许可证文本及第三方组件声明均予以保留。
