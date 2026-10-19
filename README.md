# Arch Linux 原生 Wine 企业微信

复现已验证的 **企业微信 5.0.11.6018 + 原生 Wine + x64 CEF 5.0.79.5483**，并保留安装理由与维护方法。

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

## 功能与验证状态

| 项目 | 部署与验证 |
|---|---|
| 页面、字体与中文输入 | 30 文件 CEF、winecfg 覆盖和 fcitx5 桥接已用于日常环境；聊天、文档、邮件页面及常用中文输入已验收 |
| 任务栏与托盘菜单 | 任务栏恢复、最小化时显示菜单和二级菜单焦点修复已部署；用户已确认菜单位置、子菜单保持、点击外部关闭及主窗口保持最小化。见[修复记录](verification-and-recovery.md#最小化托盘菜单与二级菜单) |
| CEF 内存修复 | 页对齐版已部署，CEF 匿名映像由每进程约 232 MiB 降至约 15 MiB；修改 DLL 会使签名失效，不随启动器自动执行。见[验证与回退](experiments/cef-memory/README.md) |
| Wine 内存修复 | 专用 11.19 vprot 运行时已部署，避免每个 1 TiB 预留区对应的约 256 MiB 元数据驻留；7 个 renderer 的正式样本总 PSS 约 2.94 GiB。见[运行时安装与边界](experiments/wine-vprot/README.md) |
| 慢启动与启动器 | 两个助手的进程存活等待已随窗口／托盘修复部署，真实慢启动仍未验收；仓库启动器的相对 prefix 与可移植运行时配置尚未成套同步。见[启动路径与等待](verification-and-recovery.md#启动路径与慢启动等待) |
| 完整 IME 提交 | 整段入队方案仍为[隔离实验](experiments/wine-ime-commit/README.md)，未接入日常 Wine；生产桥接仍逐字提交 |

内存样本的页面负载不同，不能相减计算整体收益。邮件收发、长期稳定性和完整从零安装尚未验收，详细范围及证据见[使用与维护](verification-and-recovery.md#验证范围)。文档预加载保持不变。

## 当前环境

- 企业微信 **5.0.11.6018**；安装材料对应 Wine **11.18**，启动和窗口修复在 **11.19** 下验证。
- 当前正式实例使用企业微信专用 Wine **11.19 vprot** 运行时；其他模块和 wineserver 复用同版本系统包，不替换全局 Wine。
- Arch Linux，Wayland/XWayland；基础配置验证环境为 Plasma/KWin **6.7.5**，任务栏与托盘修复在 **6.8.0** 下验证；双屏分别为 **135% / 100%** 缩放。
- 原生环境：`~/Works/Softwares/wine/WeCom`，桌面入口 `WeCom-Native-Wine.desktop`。

## 两种已验证配置

| 方案 | CEF 文件数 | 依赖与状态 |
|---|---|---|
| winetricks 方案 | **78** | 11 个运行库由 `winetricks vcrun2022 ucrtbase2019 d3dcompiler_47` 提供；保留清单供对照及回退 |
| 当前正式方案 | **30** | 在 78 文件方案基础上，再省去 46 个 API-set 文件，并使用 Wine 内建的 `vcruntime140_1`、`dbghelp`；独立试验及正式环境重启后的页面、输入和显示均通过验收 |

两种方案均保留主程序、WeMail 安装器自带的 DLL 和 manifest。30 文件方案的配置、限制及证据见 [winecfg 试验](materials/winetricks.md#winecfg-覆盖试验)；校验／打包工具支持 `full`（89）、`winetricks`（78）和 `wine-builtin`（30）三种清单。正式迁移证据见 [部署记录](materials/winecfg-deployment.json)。

已有安装更新辅助脚本时使用[维护步骤](verification-and-recovery.md#更新启动辅助文件)。30 文件正式切换保留了 48 个移出文件、原覆盖配置和恢复注册表值，供回退至 78 文件方案使用；后续内存和窗口修复的备份见各自维护记录。

## 仓库内容

| 目录 | 内容 |
|---|---|
| [configs/](configs/) | Qt、字体注册表、DLL 加载配置 |
| [materials/](materials/) | 安装包、CEF、RichEdit、字体的版本与校验清单 |
| [scripts/](scripts/) | 材料校验、启动及兼容辅助工具；两个 EXE 和输入桥接 SO 均附源码 |
| [examples/](examples/) | 修复文件索引、桌面入口示例、第三方素材来源和校验信息 |
| [experiments/wine-vprot/](experiments/wine-vprot/) | 已部署内存补丁、探针源码、验证证据及运行时复建边界 |
| [experiments/cef-memory/](experiments/cef-memory/) | 已部署 CEF 页对齐修复的探针、数值证据及回退说明 |
| [experiments/wine-ime-commit/](experiments/wine-ime-commit/) | 未部署的整段输入提交实验及测试边界 |
| [tests/](tests/) | 离线回归测试；运行方法见[仓库验证](verification-and-recovery.md#仓库验证) |

自制兼容工具提供源码、配置与构建方法；第三方 CEF、微软 DLL、字体和安装器不上传，也不通过 Release 分发。来源和获取步骤见[素材说明](examples/README.md)，取得后放入被 Git 忽略的 `assets/`。CEF 需从企业微信下载的匹配组件或已有已验证环境提取，本项目没有该历史组件的固定官方下载直链。

RichEdit 使用 `winetricks riched30`，西文字体使用 winetricks，四个中文 OTF 从 Adobe 获取，详见[组件对照与验证](materials/winetricks.md)。这些材料不另行上传，安装程序仍需自行准备。账号数据、原始诊断材料及历史文档归档不随仓库分发。文档中的 `/home/<user>` 须替换为实际路径。
