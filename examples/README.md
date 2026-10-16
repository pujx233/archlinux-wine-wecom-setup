# 修复示例与素材来源

本目录对应企业微信 **5.0.11.6018 / 系统 Wine 11.19 / fcitx5 / KDE Wayland** 的已验证修复。自制工具提供源码、配置和构建方法；第三方组件只记录来源、获取方式和校验值，不上传二进制，不通过 Release 分发。此前的 CEF Release 已移除。

2026-10-16 正式实例进一步使用 [Wine vprot 专用运行时](../experiments/wine-vprot/README.md)降低页保护元数据占用；系统 Wine 未被替换。补丁、探针、固定指纹安装器和回退均已入库，Wine 二进制及账号数据不随仓库分发。

## 自制工具与配置

| 修复 | 项目源码与配置 |
|---|---|
| 邮件、文档正文和审批选词不出字 | [ime-bridge.c](../scripts/ime-bridge.c)，由[启动器](../scripts/launch-wecom.sh)加载 |
| 文档列表无法嵌入 | [qt.conf](../configs/qt.conf) 和[加载顺序](../configs/dll-overrides.txt) |
| 中文字体与提示 | [字体注册](../configs/fonts-install.reg)、[字体替换](../configs/fonts-replacements.reg)、[StatusFont 工具源码](../scripts/status-font-probe.c) |
| 五分钟启动等待 | [prepare-startup.py](../scripts/prepare-startup.py) |
| 标题栏、拖动和托盘菜单交互 | [titlebar-managed.c](../scripts/titlebar-managed.c)、[tray-geometry.py](../scripts/tray-geometry.py) |
| 托盘提醒闪黑块 | [tray-icon.py](../scripts/tray-icon.py) |
| 桌面入口 | [WeCom-Native-Wine.desktop](WeCom-Native-Wine.desktop)，替换 `<user>` 和 prefix 路径 |
| 材料核验与本地归档 | [verify-materials.py](../scripts/verify-materials.py)、[package-cef-example.py](../scripts/package-cef-example.py) |
| winecfg 加载顺序与清单重定向试验 | [DLL 覆盖配置、探针源码和结果](dll-overrides/README.md)，已由独立试验应用到正式环境 |

仓库中的小型 EXE/SO 均为上述源码的构建产物；重建命令、开关和维护方式见[辅助文件维护](../verification-and-recovery.md#辅助文件维护)。它们不包含第三方 CEF、微软 DLL 或字体。

## 第三方素材从哪里获取

| 素材 | 来源和获取方式 | 本项目验证版本 |
|---|---|---|
| 企业微信 Windows 安装器 | [企业微信官方客户端下载入口](https://work.weixin.qq.com/#indexDownload)；精确复现需保留对应历史安装包并核对 [SHA256](../materials/versions.json) | 5.0.11.6018 |
| 腾讯配套 x64 CEF | 企业微信客户端自行下载的 Web 组件；从已取得该版本的客户端组件目录或已有已验证环境中提取，详见下方步骤 | 5.0.79.5483 / Chromium 129.0.6668.101 |
| VC14、UCRT、D3DCompiler | [Winetricks 上游配方](https://github.com/Winetricks/winetricks/blob/master/src/winetricks)从微软来源获取；执行 `vcrun2022 ucrtbase2019 d3dcompiler_47` | 版本与例外见[验证记录](../materials/winetricks.md#独立验证与精简范围) |
| x86 RichEdit | `winetricks riched30` 获取微软安装包并安装；不要按 DLL 名选择 `riched20` | 5.30.23.1200 |
| 西文字体 | `winetricks corefonts calibri consolas tahoma micross` | 与原快照的版本差异见[字体说明](../materials/winetricks.md#字体安装) |
| Source Han Sans CN | Adobe [Source Han Sans 2.005R](https://github.com/adobe-fonts/source-han-sans/releases/tag/2.005R)，CN 子集 Regular/Bold OTF | 2.005 |
| Source Han Serif CN | Adobe [Source Han Serif 2.003R](https://github.com/adobe-fonts/source-han-serif/releases/tag/2.003R)，CN 子集 Regular/Bold OTF | 2.003 |

官网当前安装器不保证仍为上述历史版本。本项目没有已验证、可长期使用的 **CEF 5.0.79.5483 独立官方下载直链**。普通 Chromium CEF 或 WebView2 不能替代企业微信配套组件；如果客户端没有下载这一版本，或文件校验不符，需要另行取得匹配来源或重新验证新版本，不能只改目录名沿用结论。

## 在本地准备 CEF 示例素材

来源可为已验证版本的原厂 89 文件、旧 78 文件或当前正式的 30 文件目录，例如：

```text
<原 prefix>/drive_c/Program Files (x86)/WXWork/5.0.11.6018/compatible_web/
```

这是经过 x64 组件替换后的目录；安装器最初的同名目录可能是 x86，不能直接当作来源。其他客户端的下载缓存位置可能不同，应定位 `WXWorkWeb.exe` 与 `libcef.dll` 所在的成套目录，再按对应清单核对文件与 x64 架构。当前日常环境是 30 文件来源；按来源选择 `wine-builtin`（30）、`winetricks`（78）或 `full`（89）校验。

在仓库根目录的 Bash 中执行。打包器只读来源目录，输出到本地 `dist/`，不修改现有企业微信：

```bash
set -euo pipefail
export WECOM_CEF_SOURCE='/path/to/verified/compatible_web'
export WECOM_CEF_SOURCE_PROFILE=wine-builtin  # 30 文件；78 用 winetricks，89 用 full
python3 scripts/verify-materials.py --only cef --cef-profile "$WECOM_CEF_SOURCE_PROFILE" \
  --cef-dir "$WECOM_CEF_SOURCE"
mkdir -p dist assets
python3 scripts/package-cef-example.py --cef-profile wine-builtin \
  --cef-dir "$WECOM_CEF_SOURCE" \
  --output dist/wecom-cef-x64-5.0.79.5483-wine-builtin-local.tar.xz
test ! -e assets/cef
tar --extract --xz --no-same-owner \
  --file dist/wecom-cef-x64-5.0.79.5483-wine-builtin-local.tar.xz
python3 scripts/verify-materials.py --only cef --cef-profile wine-builtin
```

解压得到 `assets/cef/`，共 **30 个文件**。打包时跳过[明确列出的 59 个文件](../materials/cef64-wine-builtin-profile.json)，其余文件逐一按[厂商清单](../materials/cef64-vendor-manifest.json)核验并原样保留。新 prefix 必须按[安装步骤第 5 节](../install-from-scratch.md#5-配置运行库及-qt-dpi)用 winetricks 提供运行库，并导入 `cef-builtin.reg` 配置。

主程序和 `wemail/` 的安装器自带 DLL 继续保留。30 文件方案已在独立 prefix 和正式环境重启后验证邮件、文档、工作台和中文输入；详细边界见[验证记录](../materials/winetricks.md#winecfg-覆盖试验)。

[bundle.json](cef/bundle.json) 与 [SHA256SUMS](cef/SHA256SUMS) 记录此前 78 文件方案的本地归档，不能用来校验 30 文件归档；[full-bundle.json](cef/full-bundle.json) 与 [full-SHA256SUMS](cef/full-SHA256SUMS) 记录原完整归档。它们是校验记录，不是下载链接。不同 Python/xz 版本可能生成不同压缩包哈希，解压后的文件级校验才是内容一致性的依据。打包器也接受已经精简的目录，省略 `--cef-profile` 时按完整清单打包，现有输出文件不会被覆盖。

`assets/`、`dist/`、账号资料和原始日志均被 Git 忽略。第三方组件属于各自权利人，不由本项目重新授权。
