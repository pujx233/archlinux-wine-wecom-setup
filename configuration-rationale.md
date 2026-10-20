# 配置说明

本页说明 **企业微信 5.0.12.6018 / Wine 11.20** 的正式修复及原因。版本以[版本清单](materials/versions.json)为准，操作见[从零安装](install-from-scratch.md)与[使用维护](verification-and-recovery.md)。

## 运行环境

使用独立 `win64` prefix，使 x86 主程序与 x64 Web 子进程共存。[启动器](scripts/launch-wecom.sh)将 prefix 解析为绝对物理路径，避免切换工作目录后相对路径失效。当前使用系统 Wine 11.20；专用运行时仅在版本和固定哈希均匹配时启用。

`LANG/LC_CTYPE/LC_MESSAGES=zh_CN.UTF-8` 并清除 `LC_ALL`，统一语言和字体初始化。`winemenubuilder.exe=d` 禁用 Wine 自动生成菜单，使用加载完整配置的专用桌面入口。

## 文档页面

| 配置 | 解决的问题与原因 |
|---|---|
| 整套 x64 CEF 5.0.79.5483 | x86 CEF 的 V8 页面属性检查与 Wine 返回值不兼容；成套替换程序、DLL、资源和 locales，恢复网页组件运行 |
| 微软 VC/UCRT 优先加载 | 提供企业微信所需的运行库接口，避免 Wine 内建实现缺少导入 |
| WeMail 的 `dpiawareness=1` | 主窗口和 Qt 子窗口的 DPI 感知模式不一致会导致嵌入失败；统一模式后可显示文档列表及正文 |

正式 CEF 保留 30 个文件：11 个运行库由 winetricks 提供，`vcruntime140_1`、`dbghelp` 使用 Wine 内建实现，46 个 API-set 文件由系统解析。主程序和 WeMail 保留原始 DLL 与 manifest，因为其清单固定了本地加载路径。组件选择见[运行库说明](materials/winetricks.md)。

[dll-overrides.txt](configs/dll-overrides.txt)提供环境覆盖，[cef-builtin.reg](configs/cef-builtin.reg)为三个 CEF 程序选择内建实现；`n,b` 表示优先原生实现并保留内建回退。[qt.conf](configs/qt.conf)只放在版本目录的 `wemail/`，无需改变全局缩放。升级后须恢复对应版本目录的配置。

## 字体与输入

| 层次 | 配置与原因 |
|---|---|
| 字体文件 | winetricks 西文字体加四个思源 CN OTF，提供所需字形 |
| GDI | `FontSubstitutes` 映射字体名称，`FontLink\SystemLink` 补充中文字形 |
| DirectWrite | `Fonts/Replacements` 映射应用请求的字体族，覆盖水印等独立渲染路径 |
| 编辑框 | 微软 x86 RichEdit 5.30.23.1200 与 `riched20=n,b` 修复编辑中的中文方框 |
| 悬浮提示 | 将独立的 `WindowMetrics\StatusFont` 设为 `Source Han Sans CN`，解决提示缺字 |

字体注册与映射由 [fonts-install.reg](configs/fonts-install.reg)、[fonts-replacements.reg](configs/fonts-replacements.reg)提供。思源字体用于替代请求的中文字体族，不等于安装微软原版字体。

[set-status-font.c](scripts/set-status-font.c)通过 Windows API 设置提示字体，保留字号、字重和其他窗口参数。完整退出并重启企业微信后，提示控件采用新配置。RichEdit 由 `winetricks riched30` 安装到 `windows/syswow64/`，并保留配套 `msls31.dll`。

## 内嵌页面与邮件的 fcitx5 提交

Wine 的 XIM 提交可能交给外层 Qt 宿主，而编辑焦点位于另一进程的 CEF 子窗口，表现为有候选框却不出字。字体、RichEdit 和 TSF 组件不能修复这条跨进程提交路径。

[ime-bridge.c](scripts/ime-bridge.c)仅在企业微信启动链加载。宿主必须为 WeMail 或主程序，焦点必须属于嵌入的 CEF 编辑器，输入事件窗口必须与宿主匹配，才向编辑器投递 Unicode `WM_CHAR`；其他输入保持 Wine 行为。该配置覆盖邮件正文、文档正文和审批编辑框，开关为 `WECOM_IME_FIX`。

桥接不修改剪贴板或系统 DLL。它依赖 Wine 内部接口和 CEF 窗口类别，适用于 Arch x86_64 新 WoW64；升级后需检查兼容性。模块路径不能包含空白或冒号。逐个投递 UTF-16 字符时，途中失败不能保证整段提交完整。

## CEF 映像内存共享

原始 `libcef.dll` 的文件段按 512 字节对齐，Wine 无法将部分映像直接映射为可共享文件页，会为各进程保留匿名副本。[align-cef-memory.py](scripts/align-cef-memory.py)将已知原件重排为 4096 字节对齐，保留 RVA 和代码内容，并修正文件偏移。

页对齐使 CEF 匿名映像占用由每进程约 232 MiB 降至约 15 MiB；应用总内存仍受页面数量和内容影响。工具只生成独立副本，拒绝未知源、原地修改及覆盖输出。厂商签名会失效，原件必须保留；`--cef-layout page-aligned` 仅对该 DLL 使用[独立指纹](materials/cef64-page-aligned.json)，其余文件仍按厂商清单校验。

安装步骤显式应用此修复，日常启动不改写 DLL，也不关闭文档预加载。回退方式见[维护说明](verification-and-recovery.md#cef-映像内存共享)。

## Wine 页保护元数据

Wine 11.19 在 CEF/V8 预留 1 TiB 无访问地址时，会将约 256 MiB 页保护元数据写入物理内存。[正式补丁](patches/wine-11.19-vprot.patch)仅对独立匿名元数据中完整的宿主页使用 `MADV_DONTNEED` 恢复按需零页；边缘、非零值及失败路径仍用 `memset`，不修改 V8 地址区或沙箱。

该补丁仅适用于固定的 Wine 11.19 运行时；**当前 Wine 11.20 不加载它**。启动器检查固定的候选及系统依赖哈希，不匹配时使用系统 Wine。专用 loader、preloader 和 ntdll 保存在用户目录，系统 Wine 不被替换。版本限制与回退见[维护说明](verification-and-recovery.md#wine-页保护元数据)。

## 启动等待

失效的 Boost 共享锁会导致网页组件等待初始化。[prepare-startup.py](scripts/prepare-startup.py)只处理 `WXWORK-TBS-HELPER-LOCK` 中长 12 字节、状态为 0 或 1 的普通文件，并在确认目标 prefix 空闲后移动到备份目录；正常状态、符号链接和活动状态不明时均跳过。它不清理聊天数据或登录标记。

窗口助手按目标应用进程存活状态等待主窗口，避免慢启动超过固定期限后失效。最初 120 秒始终未确认进程存在时结束等待；确认存在后持续等待，只有连续确认进程不存在满 6 秒才退出。短暂进程交接或状态未知不视为退出。标题栏绑定后只跟踪同一进程的窗口；托盘助手同时核对用户、参数和 prefix。

## 标题栏与边缘缩放

企业微信标题栏与分层阴影是独立窗口。[窗口助手](scripts/titlebar-managed.c)对主窗口所属 `TitleBarWindow` 增加 `WS_SYSMENU` 并清除 `WS_EX_NOACTIVATE`，使标题栏受桌面管理且可点击激活；对主窗口阴影增加 `WS_EX_TRANSPARENT`，将边缘拖动交给主窗口。

助手不周期性抢焦点，也不强制窗口尺寸；同一 prefix 只运行一个实例，随主进程退出。缩小仍受应用自身最小尺寸约束。

## 任务栏最小化恢复

X11 与 Win32 最小化状态不同步会使任务栏激活后窗口再次最小化。助手仅在当前绑定主窗口成为前台、可见、启用且仍最小化时，异步投递 `WM_SYSCOMMAND / SC_RESTORE`。其他窗口、过时事件和正常隐藏操作均跳过。

## 多屏托盘菜单与悬浮提示

KDE Wayland 与 Wine/XWayland 的坐标原点和缩放不同。[tray-geometry.py](scripts/tray-geometry.py)按输出名称匹配 KScreen 与 XRandR，生成逐屏映射；菜单和托盘提示经换算后限制在对应工作区内，保留大小与焦点。

助手核对当前 prefix、窗口归属及托盘位置，排除应用内部提示和气泡通知。依赖缺失、输出匹配不唯一或布局检查不符时跳过；改变屏幕排列或缩放后需重新启动应用。

## 最小化时的托盘菜单

KWin 会将不可见主窗口的附属菜单一同最小化，但 Wine 菜单的 Win32 状态可能不同步。窗口助手只对当前主窗口拥有、刚由托盘打开的菜单发送请求；[tray-icon.py](scripts/tray-icon.py)校验 prefix、进程、归属、时效和矩形后，通过 X11 显示并定位该菜单。

请求仅含窗口 ID、序号和矩形，不含聊天内容。主窗口保持最小化，菜单关闭沿用应用行为。此功能需要 `WECOM_TITLEBAR_FIX` 与 `WECOM_TRAY_ICON_FIX` 均启用。

## 托盘菜单关闭

子菜单阴影获得焦点时仍属于同一菜单组；助手沿 owner 链识别菜单及阴影，避免误关闭根菜单。点击组外或前台离开菜单组时，向根菜单投递 `WM_CLOSE`，由应用清理菜单和阴影。

Wine 切换到桌面时可能不发送前台事件，因此仅在菜单打开期间补充前台状态检查。该处理不影响菜单内操作，由 `WECOM_TITLEBAR_FIX` 控制。

## 托盘消息提醒

Wine 的 20×20 图标空白帧可能显示为居中的 16×16 黑块。托盘助手用 XDamage 监听重绘，只替换符合该尺寸、像素和归属条件的帧，保留正常图标与闪烁提醒。

空白占位使用 1/255 透明度，避免 KDE 丢弃全透明更新，保留原点击区域。图标归属或像素不符时不修改。

## 托盘点击激活

托盘属于 Wine Explorer，点击恢复后前台可能仍留在托盘窗口。窗口助手在左键释放后确认主窗口已可见、未最小化且前台仍为该托盘，再短暂连接前台输入队列、激活主窗口并分离。用户已经切换窗口时不强制激活。

## 聊天文件菜单与表格打开

聊天文件菜单的硬件点击可能激活 `WS_EX_NOACTIVATE` 菜单，导致菜单在命令执行前销毁。助手仅对同进程、owner 链回到当前前台主窗口的菜单转发按下和释放消息；托盘菜单、其他进程和普通鼠标操作不受影响。菜单在按下时销毁后，配对释放被消耗，避免误点新对话框。

xls/xlsx 的[原生关联](configs/native-spreadsheet-open.reg)通过 Wine 的 `winebrowser.exe` 交给宿主默认应用，正式配置使用 WPS 表格。该关联按需导入，不由启动器反复覆盖。导入及回退见[维护说明](verification-and-recovery.md#聊天文件菜单与原生表格打开)。
