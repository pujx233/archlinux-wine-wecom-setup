# Arch Linux 原生 Wine 企业微信

维护 **企业微信 5.0.12.6018 + 系统 Wine 11.20 + x64 CEF 5.0.79.5483** 的正常使用配置、功能修复与回退方法。安装、升级及修复说明统一对应[当前版本清单](materials/versions.json)。

## 版本与提交约定

1. 企业微信更新后，所有正式修复、安装步骤、默认校验和使用示例均以最新正常使用版本为准，在同一提交中同步。CEF、Wine 等兼容组件按应用需要固定版本，保留版本和哈希检查。
2. commit 标题和说明描述正常使用版本及正式修复。setup 文档只保留最终配置、使用结果、原因和必要的操作说明，不记录调试过程或时间线。
3. 调试记录、探针、实验代码和本地测试保存在 Git 忽略的 `.local-debug/` 中，不纳入提交。正式修复源码、配置和材料校验清单继续纳入版本控制。

| 需要做什么 | 文档 |
|---|---|
| 从空 Wine prefix 安装 | [从零安装](install-from-scratch.md) |
| 了解修复及其原因 | [配置说明](configuration-rationale.md) |
| 启动、更新或回退 | [使用与维护](verification-and-recovery.md) |
| 查找正式源码及素材 | [修复示例与来源](examples/README.md) |
| 选择运行库和字体 | [winetricks 组件说明](materials/winetricks.md) |

```bash
git clone https://github.com/tbsangel/archlinux-wine-wecom-setup.git
cd archlinux-wine-wecom-setup
```

## 正式功能修复

| 功能 | 最终配置及作用 |
|---|---|
| 文档、邮件及工作台 | 成套 x64 CEF、微软运行库及 Qt DPI 配置，解决网页组件崩溃和子窗口嵌入失败 |
| 中文显示与输入 | 思源字体、GDI/DirectWrite 映射、RichEdit、提示字体设置和 fcitx5 提交桥接 |
| 启动等待 | 清理符合条件的失效初始化锁，辅助程序按应用进程存活状态等待主窗口 |
| 标题栏与任务栏 | 修正标题栏管理、阴影鼠标穿透及最小化后的任务栏恢复 |
| 托盘 | 逐屏坐标换算、最小化菜单显示、二级菜单保持、外部点击关闭及消息空白帧修复 |
| 聊天文件菜单 | 转发菜单点击以支持另存为、文件及文字复制；xls/xlsx 交给原生 WPS 打开 |
| CEF 内存 | 对已知 libcef.dll 执行页对齐，使可共享映像使用文件映射；保留厂商原件供回退 |
| Wine 运行时 | 当前使用系统 Wine 11.20；版本守卫禁止加载仅适用于 11.19 的专用 vprot 运行时 |

各项原因和适用边界见[配置说明](configuration-rationale.md)。文档预加载保持启用。

## 当前环境

- 企业微信 **5.0.12.6018**、系统 Wine **11.20**，组件指纹见[配置清单](materials/app-upgrade.json)。
- Arch Linux、Plasma/KWin **6.8.0**、Wayland/XWayland，双屏缩放 **135% / 100%**。
- 正式 prefix 为 `~/Works/Softwares/wine/WeCom`，桌面入口为 `WeCom-Native-Wine.desktop`。
- 正式 CEF 为 **30 文件 + 页对齐**，使用应用专用内建覆盖；主程序和 WeMail 保留安装器提供的 DLL 与 manifest。

## 仓库内容

| 目录 | 内容 |
|---|---|
| [configs/](configs/) | Qt、字体、文件关联、DLL 覆盖及运行时指纹 |
| [materials/](materials/) | 正式组件选择、版本和校验清单 |
| [scripts/](scripts/) | 安装、启动及兼容修复工具，EXE/SO 均附源码 |
| [patches/](patches/) | 有版本约束的正式修复补丁 |
| [examples/](examples/) | 桌面入口、正式源码索引及素材准备方法 |

第三方 CEF、微软 DLL、字体和安装器不上传，也不通过 Release 分发；取得后放入 Git 忽略的 `assets/`。CEF 需从匹配客户端组件或已有环境提取。账号数据、原始日志和本地归档不随仓库分发。文档中的 `/home/<user>` 须替换为实际路径。

## GNOME Wayland 弹窗适配

在 Ubuntu 26.04、GNOME Wayland/XWayland、Wine 11.20 和企业微信 5.0.12.6018 上，
右键菜单和联系人卡片可能只显示空框。下面两项是针对该环境的 workaround，
尚未证明具体 Wine 或 Mutter 根因，也未覆盖所有弹窗及其他桌面环境。

- `scripts/gnome-menu-fix.py` 隐藏匹配菜单的阴影，在约 100 ms 后重新映射菜单，因此会有短暂闪烁。
  已通过右键菜单的剪切、粘贴实际操作；不代表所有菜单项均已验证。
- `scripts/gnome-popup-fix.c` 给匹配的 `UserInfoWindow` 补主窗 owner，并执行
  `FRAMECHANGED | NOACTIVATE`，将该卡片直接拥有的 `PerryShadowWnd` 装饰阴影 alpha 设为 0。
  这会隐藏装饰阴影。已测试三个不同成员、再次悬浮和主窗最小化/恢复后重新打开，共五个新卡片窗口；
  卡片正常显示且没有异常阴影，操作前后的前台窗口均保持主窗。尚未覆盖卡片内所有按钮和输入路径。

启动器只在 GNOME Wayland 且存在 `DISPLAY` 时启用两项助手。菜单助手校验 `WECOM_PREFIX`、
进程身份和窗口归属链；卡片助手在该 Wine prefix 内校验 `WXWork.exe` 主窗、进程句柄、
卡片类型及直接阴影归属。两项各自保证每个 prefix 只有一个实例。主进程退出后会重新查找主进程，
最多等待 120 秒，以覆盖自动重新登录；没有匹配进程则退出。

设 `WECOM_GNOME_MENU_FIX=0` 或 `WECOM_GNOME_POPUP_FIX=0` 可分别关闭。
日志位于 `$WECOM_PREFIX/wecom-gnome-menu.log` 和 `$WECOM_PREFIX/wecom-gnome-popup.log`。
关闭开关影响下次启动；已经启动的助手应随该 prefix 一并停止。
源码与编译后的助手一并提供，卡片助手可用 MinGW-w64 重新编译：

```sh
i686-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror scripts/gnome-popup-fix.c \
  -o scripts/gnome-popup-fix.exe -luser32
```
