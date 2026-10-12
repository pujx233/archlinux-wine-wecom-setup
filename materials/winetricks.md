# 当前修复与 winetricks 组件对照

2026-10-12 在全新独立 `win64` prefix 中实际安装并验证了以下组件，使用系统 Wine 11.19、企业微信 5.0.11.6018 和 x64 CEF 5.0.79.5483。用户手动登录，未复制日常账号状态；独立试验阶段未改动日常 prefix，391 个受保护文件的哈希未变。随后经用户授权，日常环境也完成切换与验收，见下方记录。历史文件比对仍见 [winetricks-comparison.json](winetricks-comparison.json)。

本机 winetricks 脚本 SHA256：`672a1ff4442e8691a3ffc0e6860137e201a1d1227e9b3044245f1731e9e9837e`。配方可在 [Winetricks 上游源码](https://github.com/Winetricks/winetricks/blob/master/src/winetricks)中按表内名称查找；后续配方和下载版本可能变化。逐文件核对结果见 [winetricks-comparison.json](winetricks-comparison.json)。

本文将 **winetricks 安装微软组件** 与 **winecfg 选择 Wine 内建实现** 分开记录。78 文件方案使用前者；当前正式环境在此基础上采用后者，保留 30 个 CEF 文件，已完成独立试验和正式重启验收。配置选择见 [README](../README.md#两种已验证配置)。

## 微软 DLL

| 文件/用途 | 对应 winetricks 包 | 核对结论 |
|---|---|---|
| x86 `riched20.dll` 5.30.23.1200，修复聊天编辑框方框 | **`riched30`** | 从该配方的 `InstMsiA.exe` 提取出的 DLL 与当前文件 SHA256 完全相同；不要按 DLL 名误选 `riched20`，后者从 Windows 2000 SP4 取另一版本 |
| `msls31.dll`，RichEdit 的 Line Services | 随 `riched30` 安装；另有 `msls31` / `msls31_nt4` | `riched30` 已复制配套文件，不必再叠装其他版本；本项目原修复只替换 RichEdit，未独立要求增加其覆盖项 |
| `msvcp140`、`msvcp140_1`、`msvcp140_2`、`msvcp140_atomic_wait`、`msvcp140_codecvt_ids`、`concrt140`、`vcruntime140`、x64 `vcruntime140_1`、`vcruntime140_threads` | **`vcrun2022`** | 属于同一 VC14 运行库系列；主程序原件为 x86 14.38.33135.0，CEF 原件为 x64 14.38.33130.0；本机该配方缓存为 14.44.35211.0，文件并不相同。缓存包也包含 `vcruntime140_threads`，但配方未单列其覆盖项 |
| CEF 原厂 `vccorlib140.dll` | `vcrun2022` | 原件为 14.38.33130.0；正式精简方案已由 winetricks 提供，无需另行收集 |
| `ucrtbase.dll` | **`ucrtbase2019`** | 主程序原件是 10.0.10240.16384，CEF 原件是 10.0.22621.2428；实装的 x86/x64 均为 10.0.10586.15，不能当作相同版本。所核对 `vcrun2019` 配方会调用它，`vcrun2022` 配方不会 |
| CEF 的 `api-ms-win-crt-*` | VC/UCRT 的配套 API-set 转发文件 | 早期 `vcrun2015/2017` 配方包含此类覆盖；不应把它们当作独立应用插件逐个安装，也不要为凑文件叠装多个 vcrun 年份 |
| CEF 的 `api-ms-win-core-*`、`api-ms-win-downlevel-*` | 没有与这套厂商目录一一等价的单个包 | 原 78 文件方案保留；当前 30 文件方案由 Wine 的 API-set 解析替代，见下方试验记录 |
| CEF 的 x64 `d3dcompiler_47.dll` 10.0.22621.3233 | **`d3dcompiler_47`** | 本次实际安装的 x86/x64 均为 6.3.9600.16384；精简 CEF 使用系统目录中的这个版本，GPU 子进程实际加载路径已核对 |
| CEF 的 x64 `dbghelp.dll` 10.0.22621.1778 | `dbghelp`（不等价） | 配方提取 XP SP3 的 x86 文件，不能作为这份 x64 文件的来源；Wine 内建版本另有独立试验，见下方记录 |
| 邮件/文档/审批的输入提交 | `msctf`（不等价） | 配方提供 XP x86 TSF 组件，未修复已定位的 Wine XIM 跨进程焦点问题；继续使用已验证的输入桥接 |
| `WXWorkWeb.exe` / `libcef.dll` | 没有对应包；`webview2` 不等价 | 企业微信此处使用 CEF；Edge WebView2 运行时不能替代这套组件 |

78 文件及当前 30 文件 CEF 方案均需在目标独立 prefix 中安装：

```bash
env WINEPREFIX='/path/to/test-prefix' WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q vcrun2022 ucrtbase2019 d3dcompiler_47
```

winetricks 向 Windows 系统目录安装文件，应用目录内仍存在的原生 DLL 可能继续优先加载。78 文件清单省去 [11 个运行库](cef64-winetricks-profile.json)；30 文件清单另省去 48 个文件，见下方 winecfg 记录。两种方案均保留安装器提供的主程序目录及 `wemail/` DLL。`vcrun2015/2017/2019/2022/2026` 是替代关系，配方声明冲突，不应全部叠装。运行库包也不能代替 Qt DPI、窗口/托盘或输入桥接修复。

### 其他安装器自带组件

检查应用目录后还确认了以下对应关系。它们由企业微信安装器提供，不是需要额外收集的素材；本轮只核对配方与文件版本，未安装这些包或验证删除应用原件后的功能。

| 应用自带文件 | 对应包 | 当前文件版本 |
|---|---|---|
| `msvcp100.dll`、`msvcr100.dll`、`atl100.dll` | `vcrun2010` | x86 10.0.40219.1；部分 MSVCP/MSVCR 副本为 10.0.40219.325 |
| `msvcp120.dll`、`msvcr120.dll` | `vcrun2013` | x86 12.0.21005.1 |
| `D3DX9_43.dll` | `d3dx9_43` | x86 9.29.952.3111 |
| `d3dx11_43.dll` | `d3dx11_43` | x86 9.29.952.3111 |
| `GdiPlus.dll` | `gdiplus` | x86 6.1.7601.23265；配方来源版本不同 |

Qt、腾讯配套 CEF、ANGLE／SwiftShader 及业务插件没有可直接替换这套厂商组件的 winetricks 包。不能只根据 DLL 同名判断兼容性。

## 独立验证与精简范围

实装命令为 `winetricks -q vcrun2022 ucrtbase2019 riched30 d3dcompiler_47 corefonts calibri consolas tahoma micross`，返回 0。另保留四个 Adobe CN OTF，导入项目字体映射并设置 StatusFont。RichEdit 哈希与原修复完全相同；西文字体采用 winetricks 版本后，用户确认中文输入和显示正常。

首次试验同时移走了主程序、WeMail、CEF 中的 32 个 VC/UCRT/D3D 文件，发现**不能只安装 winetricks 后直接删除全部应用本地 DLL**：

- 主程序的外部版本清单固定了本地 DLL 路径；试验中移除 7 项重定向后才能启动。
- WeMail 的内嵌清单也固定了本地路径，造成邮件和文档加载失败，工作台仍正常。把 winetricks 安装的 9 个 x86 DLL 复制到 `wemail/` 后，两页恢复；用户确认聊天、邮件标题与正文、文档正文、审批输入以及中文显示均正常。
- `vcrun2022` 没有覆盖本次 Wine 自带的 x64 `vcruntime140_1.dll`。进程实际加载的是 Wine 内置文件，不能据此声称它已由微软安装包替代。

此前 78 文件方案保留安装器自带的主程序和 WeMail DLL、原清单，以及 CEF 的原生 `vcruntime140_1.dll`。无需编辑 EXE 或 manifest，也不要求复制那 9 个试验用文件。CEF 包仅移除 11 个已由 winetricks 提供的运行库，从 89 个文件减为 **78 个**；仍保留 `dbghelp.dll`、API-set 文件、腾讯程序、CEF 资源和 locales。精简包必须与上述三个 winetricks 包配套使用。完整旧包已停止发布，原清单与校验信息保留，已有环境可自行备份作回退。

这一正式组合已在独立 prefix 完整重启，用户再次确认邮件、文档、工作台页面和中文输入正常；启动日志没有模块加载错误。进程路径确认 CEF 使用系统目录的微软 MSVCP、UCRT、VCRUNTIME 和 D3DCompiler，`vcruntime140_1` 使用保留的 CEF 原件。版本、哈希、加载路径及验证边界见[结构化记录](winetricks-validation.json)。测试副本已停止，临时诊断启动配置已还原。

这次验证覆盖独立 prefix 的登录、页面加载、中文输入和显示，以及进程实际模块路径。企业微信程序目录来自同版本已安装文件；没有重新执行企业微信安装器，也未验证邮件收发、审批提交、会议、Python 插件和长期稳定性。系统目录 DLL 能被单独加载，不等于整个应用已经通过验收。

## 日常环境迁移

独立验证通过后，用户授权切换日常 prefix，并确认编辑已保存。停止企业微信后保存完整 prefix 快照，再执行相同的九个 winetricks 包。安装返回 0，24 个系统 DLL 和 31 个字体文件的 SHA256 与测试环境一致；移走 CEF 的 11 个文件前，核对了 10,118 个保留应用、辅助工具和中文字体文件未变化。字体映射与 StatusFont 已恢复，原启动器和输入桥接保持不变。

通过原日常入口的启动器重启后，实际进程加载路径与最终测试组合一致，用户确认邮件、文档、工作台页面、中文输入及显示均正常。迁移时曾在 prefix 同级的 `WeCom.winetricks-backup-<时间>/prefix/` 保存快照，保留符号链接，不额外复制链接指向的宿主 Documents 等内容；迁移步骤不复制或替换这些外部目录。

验收后按用户要求删除了该快照、历史 `setup-backups/`、两个旧 x86 CEF 备份目录，以及临时测试 prefix、探针、日志、下载/解包缓存和本地归档。清理后 381 个正式环境关键文件哈希未变，主程序、邮件和 CEF 进程继续运行。项目只保留源码和整理后的验证记录，不保留可回退的本机快照。

## winecfg 覆盖试验

2026-10-12 又在全新独立 prefix 中测试了 winecfg 的“原生／内建”覆盖。只复制应用文件、已验证的 winetricks 系统 DLL 和字体，没有复制账号或日常注册表；用户手动登录。通过注册表写入与 winecfg 相同的 `AppDefaults/<EXE>/DllOverrides`，没有修改正式环境。探针源码、配置和使用方法见 [DLL 覆盖示例](../examples/dll-overrides/README.md)，逐项结果见 [winecfg-validation.json](winecfg-validation.json)。

| 组件 | 实测结果 |
|---|---|
| CEF 的 `vcruntime140_1.dll` | 对 `WXWorkWeb.exe` 设置 `*vcruntime140_1=builtin` 后，可移走 CEF 原件；实际加载 `/usr/lib/wine/x86_64-windows/vcruntime140_1.dll`。重启后页面和中文输入通过 |
| CEF 的 46 个 `api-ms-*.dll` | 可移走；Wine 的 API-set 机制直接解析到系统模块及已安装的 UCRT，无需逐项添加覆盖。第二轮重启后页面和中文输入通过 |
| CEF 的 `dbghelp.dll` | 使用 Wine 内建版本可启动；四个 CEF 程序／库所需的 13 个导出函数均存在。移走原件并设置应用专用覆盖后，第三轮重启的页面和中文输入通过；实际崩溃转储、报告流程未验收 |
| 主程序、WeMail 的本地 VC/UCRT | winecfg 不能绕过已观察到的清单重定向。以 `msvcp140.dll` 为探针，两种实际清单下移走本地文件后，`native,builtin`、`native`、`builtin,native`、`builtin` 均返回错误 126；关闭激活上下文的对照试验则能加载系统目录的微软 DLL。继续保留应用原件及 manifest |

三轮 UI 验证分别保留 77、31、30 个 CEF 文件，用户均确认邮件、文档、工作台页面和中文输入正常；最终 30 个文件逐一匹配原厂商清单。相对当时的 78 文件方案，另外省去 48 个文件。正式环境的 325 个受保护文件哈希未变。

该试验的两个 DLL 使用 Wine 自带实现，API-set 由 Wine 解析，无需另装 winetricks 包。测试启动器去掉了环境变量覆盖列表中的 `vcruntime140_1`，改由应用专用注册表值控制。独立试验完成后，30 文件配置又应用到正式环境。安装步骤与材料校验／打包器已同步支持 `wine-builtin` 配置。

另从已核验的 `vcrun2022` x64 安装器提取到微软 `vcruntime140_1.dll` 14.44.35211.0，单独原生加载成功。此前系统目录保留 Wine 文件是安装结果的缺口，并非包里没有该 DLL；单把 winecfg 设为“原生”不会生成缺失的微软文件。本轮没有把该提取文件安装到系统目录。

正式迁移结果见 [winecfg-deployment.json](winecfg-deployment.json)：30 个保留文件通过原厂商哈希检查，278 个受保护文件未变；实际 CEF 进程加载两个 Wine 内建 DLL，主程序和 WeMail 仍加载本地运行库，输入桥接继续加载。用户确认邮件、文档、工作台页面、聊天及各编辑框中文输入和显示正常。此次窗口出现较晚，原有辅助程序的 120 秒等待超时，已在窗口出现后补启动；延迟原因尚未确定。此次新建的单项回退备份保留在正式 prefix 的 `setup-backups/cef-builtin-<时间>/`。

验证边界：未测试邮件发送、审批提交、会议、崩溃报告、长期稳定性或其他版本；单独加载和导出函数存在不等于这些流程均兼容。试验结束后停止并清理临时 prefix、提取文件和原始日志，只保留源码与整理后的记录。

## RichEdit 安装

在新装步骤已经初始化、停止的目标 prefix 中，先保留原 x86 DLL，再执行：

```bash
env WINEPREFIX="$WECOM_PREFIX" WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q riched30
python3 "$WECOM_SETUP/scripts/verify-materials.py" --only richedit \
  --richedit "$WECOM_PREFIX/drive_c/windows/syswow64/riched20.dll"
```

当前匹配的 DLL SHA256 为 `1c2508fb55ddc459d0327f2017471545c87420443391567094e768fb34032da1`；上游安装包 SHA256 为 `536e4c8385d7d250fd5702a6868d1ed004692136eefad22252d0dac15f02563a`。启动器继续使用 `riched20=n,b`。本项目不分发这两个文件。

## 字体安装

西文字体可用 winetricks 准备：

```bash
env WINEPREFIX="$WECOM_PREFIX" WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q corefonts calibri consolas tahoma micross
```

只想安装对照表中的字体族，可将 `corefonts` 换为 `arial courier times verdana`；`corefonts` 会额外安装其他西文字体。字体文件不随本项目上传。

| 当前字体 | 对应包 | 缓存提取文件与当前清单的比对 |
|---|---|---|
| Arial / Times New Roman / Verdana，各 4 款 | `arial` / `times` / `verdana`，均含于 `corefonts` | 12 个文件的 SHA256 不同；是同名字体的另一版本 |
| Courier New，4 款 | `courier`，含于 `corefonts` | 4 个文件完全相同 |
| Calibri，4 款 | `calibri` | 4 个文件完全相同 |
| Consolas，4 款 | `consolas` | 4 个文件完全相同 |
| Tahoma，2 款 | `tahoma` | 2 个文件完全相同 |
| Microsoft Sans Serif | `micross` | 文件完全相同 |
| Source Han Sans CN Regular / Bold | `sourcehansans` 仅为相关替代 | 配方提供 2.004 TTC / `Source Han Sans SC`；当前配置要求 2.005 CN OTF / `Source Han Sans CN`，文件名、字体族不同 |
| Source Han Serif CN Regular / Bold | 所核对配方没有 `sourcehanserif` | 当前使用 2.003 CN OTF，从 Adobe 官方版本取得 |

精确复现目前中文字体映射时，从 Adobe 的 [Source Han Sans 2.005R](https://github.com/adobe-fonts/source-han-sans/releases/tag/2.005R) 和 [Source Han Serif 2.003R](https://github.com/adobe-fonts/source-han-serif/releases/tag/2.003R) 取得 CN 子集 Regular/Bold OTF，共四个文件，文件名与 SHA256 见 [fonts.json](fonts.json)。不要把 TTC 改名为 OTF；若采用 `winetricks sourcehansans`，必须同时调整映射和 StatusFont 的字体族，再验收中文显示。

`fonts.json` 继续记录原已验证环境，不将不同版本的哈希覆盖进去。采用 winetricks 的西文字体版本后，12 个文件会与历史清单不同；本次用户已确认独立环境的中文输入和显示正常。`verify-materials.py --only fonts --fonts-profile cn` 只校验四个 CN OTF；`--fonts-profile snapshot` 保留原 31 文件快照校验，不能混用。

从零安装第 6 节采用 winetricks 西文字体加四个 CN OTF，随后导入字体注册与替换配置，并执行第 8 节的 StatusFont 设置。字体与独立 RichEdit 文件仍不上传到仓库。
