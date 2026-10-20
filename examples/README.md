# 修复示例与素材来源

本目录对应最新正常使用版本 **企业微信 5.0.12.6018 / 系统 Wine 11.20 / fcitx5 / KDE Wayland** 的正式修复，版本以[当前清单](../materials/versions.json)为准。自制工具提供源码、配置和构建方法；第三方组件只记录来源、获取方式和校验值，不上传二进制，不通过 Release 分发。

正式实例使用系统 Wine 11.20。专用 11.19 vprot 运行时由版本守卫停用，运行时选择见[正式配置说明](../verification-and-recovery.md#wine-页保护元数据)。Wine 二进制及账号数据不随仓库分发。

## 自制工具与配置

| 修复 | 项目源码与配置 |
|---|---|
| 邮件、文档正文和审批选词不出字 | [ime-bridge.c](../scripts/ime-bridge.c)，由[启动器](../scripts/launch-wecom.sh)加载 |
| 文档列表无法嵌入 | [qt.conf](../configs/qt.conf) 和[加载顺序](../configs/dll-overrides.txt) |
| 中文字体与提示 | [字体注册](../configs/fonts-install.reg)、[字体替换](../configs/fonts-replacements.reg)、[StatusFont 设置工具](../scripts/set-status-font.c) |
| 五分钟启动等待 | [prepare-startup.py](../scripts/prepare-startup.py) |
| 标题栏、拖动、任务栏恢复与托盘坐标 | [titlebar-managed.c](../scripts/titlebar-managed.c)、[tray-geometry.py](../scripts/tray-geometry.py) |
| 最小化托盘菜单与二级菜单保持 | [titlebar-managed.c](../scripts/titlebar-managed.c)、[tray-icon.py](../scripts/tray-icon.py)，两个助手需成套更新 |
| 托盘提醒闪黑块 | [tray-icon.py](../scripts/tray-icon.py) |
| 桌面入口 | [WeCom-Native-Wine.desktop](WeCom-Native-Wine.desktop)，替换 `<user>` 和 prefix 路径 |
| 材料核验与本地归档 | [verify-materials.py](../scripts/verify-materials.py)、[package-cef-example.py](../scripts/package-cef-example.py) |
| winecfg 加载顺序 | [CEF 内建覆盖](../configs/cef-builtin.reg)及[正式配置说明](../configuration-rationale.md#文档页面) |

仓库中的小型 EXE/SO 均为上述源码的构建产物；重建命令、开关和维护方式见[辅助文件维护](../verification-and-recovery.md#辅助文件维护)。它们不包含第三方 CEF、微软 DLL 或字体。

## 第三方素材从哪里获取

| 素材 | 来源和获取方式 | 本项目验证版本 |
|---|---|---|
| 企业微信 Windows 安装器 | [企业微信官方客户端下载入口](https://work.weixin.qq.com/#indexDownload)；使用当前正常使用版本并核对 [SHA256](../materials/versions.json) | 5.0.12.6018 |
| 腾讯配套 x64 CEF | 企业微信客户端自行下载的 Web 组件；从已取得该版本的客户端组件目录或已有已验证环境中提取，详见下方步骤 | 5.0.79.5483 / Chromium 129.0.6668.101 |
| VC14、UCRT、D3DCompiler | [Winetricks 上游配方](https://github.com/Winetricks/winetricks/blob/master/src/winetricks)从微软来源获取；执行 `vcrun2022 ucrtbase2019 d3dcompiler_47` | 选择原因见[运行库说明](../materials/winetricks.md#cef-运行库配置) |
| x86 RichEdit | `winetricks riched30` 获取微软安装包并安装；不要按 DLL 名选择 `riched20` | 5.30.23.1200 |
| 西文字体 | `winetricks corefonts calibri consolas tahoma micross` | 与原快照的版本差异见[字体说明](../materials/winetricks.md#字体安装) |
| Source Han Sans CN | Adobe [Source Han Sans 2.005R](https://github.com/adobe-fonts/source-han-sans/releases/tag/2.005R)，CN 子集 Regular/Bold OTF | 2.005 |
| Source Han Serif CN | Adobe [Source Han Serif 2.003R](https://github.com/adobe-fonts/source-han-serif/releases/tag/2.003R)，CN 子集 Regular/Bold OTF | 2.003 |

官网安装器若更新，需同步正常使用版本、安装包指纹和全部 setup 说明。本项目没有已验证、可长期使用的 **CEF 5.0.79.5483 独立官方下载直链**。普通 Chromium CEF 或 WebView2 不能替代企业微信配套组件；如果客户端没有下载这一版本，或文件校验不符，需要另行取得匹配来源或重新验证兼容组件，不能只改目录名沿用结论。

## 在本地准备 CEF 示例素材

来源可为已验证版本的原厂 89 文件、78 文件或 30 文件目录；其中 `libcef.dll` 必须为厂商原件。例如：

```text
<正式 prefix>/drive_c/Program Files (x86)/WXWork/5.0.12.6018/compatible_web/
```

这是经过 x64 组件替换后的目录；安装器自带的同名目录可能是 x86，不能直接当作来源。其他客户端的下载缓存位置可能不同，应定位 `WXWorkWeb.exe` 与 `libcef.dll` 所在的成套目录，再按对应清单核对文件与 x64 架构。按来源选择 `wine-builtin`（30）、`winetricks`（78）或 `full`（89）校验。

日常环境已启用 CEF 页对齐修复，其 `libcef.dll` 不再匹配厂商哈希，不能直接交给下方打包器。若从日常环境提取素材，应先复制整套组件到独立材料目录，再用[原件备份](../verification-and-recovery.md#cef-映像内存共享)替换材料副本中的 `libcef.dll`，按厂商清单校验后打包。不要为准备素材覆盖正在使用的 DLL；`--cef-layout page-aligned` 仅用于校验已修复的安装，不是打包器参数。

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

主程序和 `wemail/` 的安装器自带 DLL 继续保留，因为其 manifest 固定本地加载路径，系统 DLL 不能直接替代。详见[运行库配置](../materials/winetricks.md#cef-运行库配置)。

不同 Python/xz 版本可能生成不同压缩包哈希，解压后的文件级校验是内容一致性的依据。打包器也接受已经精简的目录，省略 `--cef-profile` 时按完整清单打包，现有输出文件不会被覆盖。

`assets/`、`dist/`、账号资料和原始日志均被 Git 忽略。第三方组件属于各自权利人，不由本项目重新授权。
