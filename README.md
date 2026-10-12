# Arch Linux 原生 Wine 企业微信

复现已验证的 **企业微信 5.0.11.6018 + 原生 Wine + x64 CEF 5.0.79.5483**，并保留安装理由与维护方法。更新于 2026-10-12。

| 需要做什么 | 文档 |
|---|---|
| 从空 Wine prefix 安装 | [从零安装](install-from-scratch.md)：材料校验、配置、桌面入口 |
| 了解配置的作用与原因 | [配置说明](configuration-rationale.md)：页面、字体、启动和窗口修复 |
| 启动、排查、更新或回退 | [使用与维护](verification-and-recovery.md)：验收范围、日志、辅助工具 |
| 查找修复源码与素材来源 | [examples/](examples/)：自制工具、第三方获取方式、校验与桌面入口 |
| 对照 winetricks 可安装组件 | [组件对照](materials/winetricks.md)：RichEdit、字体、VC/UCRT 与版本差异 |
| 查看 winecfg 精简配置 | [DLL 覆盖示例](examples/dll-overrides/README.md)：30 文件 CEF、配置与加载探针 |

```bash
git clone https://github.com/tbsangel/archlinux-wine-wecom-setup.git
cd archlinux-wine-wecom-setup
```

## 当前环境

- 企业微信 **5.0.11.6018**；首次安装使用 Wine **11.18**，后续启动和窗口修复在 **11.19** 下验证。
- Arch Linux，Plasma/KWin **6.7.5**，Wayland/XWayland；双屏分别为 **135% / 100%** 缩放。
- 原生环境：`~/Works/Softwares/wine/WeCom`，桌面入口 `WeCom-Native-Wine.desktop`。

## 两种已验证配置

| 方案 | CEF 文件数 | 依赖与状态 |
|---|---|---|
| 原 winetricks 方案 | **78** | 11 个运行库由 `winetricks vcrun2022 ucrtbase2019 d3dcompiler_47` 提供；此前日常配置，保留清单供对照 |
| 当前正式方案 | **30** | 在 78 文件方案基础上，再省去 46 个 API-set 文件，并使用 Wine 内建的 `vcruntime140_1`、`dbghelp`；独立试验及正式环境重启后的页面、输入和显示均通过验收 |

两种方案均保留主程序、WeMail 安装器自带的 DLL 和 manifest。30 文件方案的配置、限制及证据见 [winecfg 试验](materials/winetricks.md#winecfg-覆盖试验)；校验／打包工具支持 `full`（89）、`winetricks`（78）和 `wine-builtin`（30）三种清单。正式迁移证据见 [部署记录](materials/winecfg-deployment.json)。

日常环境已确认邮件、文档、工作台页面、聊天及各编辑框的中文输入和显示正常；标题栏、边缘缩放及双屏托盘交互也已分别验收。输入修复使用附源码的 fcitx5 桥接，继续运行系统 Wine。邮件收发、长期稳定性和整理后的完整从零安装尚未验收，详见[验证范围](verification-and-recovery.md#验证范围)。已有安装更新辅助脚本时使用[维护步骤](verification-and-recovery.md#更新启动辅助文件)。

此前临时测试环境、诊断文件、本地归档和历史回退备份已清理。本次正式切换另保留了 48 个移出文件、原覆盖配置和恢复注册表值，供回退至 78 文件方案使用。

## 仓库内容

| 目录 | 内容 |
|---|---|
| [configs/](configs/) | Qt、字体注册表、DLL 加载配置 |
| [materials/](materials/) | 安装包、CEF、RichEdit、字体的版本与校验清单 |
| [scripts/](scripts/) | 材料校验、启动及兼容辅助工具；两个 EXE 和输入桥接 SO 均附源码 |
| [examples/](examples/) | 修复文件索引、桌面入口示例、第三方素材来源和校验信息 |

自制兼容工具提供源码、配置与构建方法；第三方 CEF、微软 DLL、字体和安装器不上传，也不通过 Release 分发。来源和获取步骤见[素材说明](examples/README.md)，取得后放入被 Git 忽略的 `assets/`。CEF 需从企业微信下载的匹配组件或已有已验证环境提取，本项目没有该历史组件的固定官方下载直链。

RichEdit 使用 `winetricks riched30`，西文字体使用 winetricks，四个中文 OTF 从 Adobe 获取，详见[组件对照与验证](materials/winetricks.md)。这些材料不另行上传，安装程序仍需自行准备。账号数据、原始诊断材料及历史文档归档不随仓库分发。文档中的 `/home/<user>` 须替换为实际路径。
