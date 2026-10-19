# 配置说明

本页按修复功能说明原因、触发条件和限制；部署及验收结果见[使用与维护](verification-and-recovery.md#验证范围)，操作命令见[从零安装](install-from-scratch.md)。

## 运行环境

使用独立 `win64` prefix，使 x86 主程序与 x64 Web 子进程共存。[启动器](scripts/launch-wecom.sh)默认使用 `/usr/bin/wine`；安装了匹配的专用 vprot 运行时后，主程序与标题栏辅助程序统一使用它。系统包版本和候选／依赖哈希不匹配时回退系统 Wine；中文区域设置和 DLL 加载顺序保持一致。

仓库启动器在切换到程序目录前，将 `WECOM_PREFIX` 解析为绝对物理路径，避免相对路径在切换目录后改变含义，影响辅助文件、日志和 Wine prefix 的定位。回归测试覆盖指向含空格目录的相对符号链接；此修复尚未同步至日常环境。

`LANG/LC_CTYPE/LC_MESSAGES=zh_CN.UTF-8` 并清除 `LC_ALL`，用于统一界面语言和字体初始化。`winemenubuilder.exe=d` 禁用 Wine 自动生成桌面菜单，统一通过专用启动入口加载配置。

## 文档页面

页面问题有两层：CEF renderer 崩溃，以及 WeMail 的 Qt 子窗口无法嵌入。仅让 renderer 存活不能恢复文档列表。

| 配置 | 原因与实际结果 |
|---|---|
| 整套 x64 CEF 5.0.79.5483 / Chromium 129.0.6668.101 | 安装包的 x86 CEF 在 V8 初始化时只接受旧页面属性 `PAGE_READWRITE (0x4)`，Wine 返回 `PAGE_WRITECOPY (0x8)` 后触发检查失败。换为原始 x64 组件后 renderer 正常，工作台能打开 |
| 配套微软 VC/UCRT 优先 | Wine 内置 MSVCP140 缺少观察到的 8 个导入，企业微信自带库有对应导出；配置后错误消失，但文档列表仍空白 |
| WeMail 的 `dpiawareness=1` | 主窗口是 System（1），Qt 窗口是 Per-Monitor（2），虽然 DPI 数值均为 96，感知模式不同仍导致嵌入失败、容器为 0×0；统一模式后列表和正文恢复 |

CEF 来自企业微信自行下载的腾讯组件，原始 89 文件清单保存在[厂商清单](materials/cef64-vendor-manifest.json)。原 78 文件方案的 11 个运行库由 winetricks 提供，详见[验证与例外](materials/winetricks.md#独立验证与精简范围)。当前正式方案在此基础上通过 [winecfg 覆盖](materials/winetricks.md#winecfg-覆盖试验)将文件数减至 30，独立试验和正式环境的页面、中文输入及显示均通过验收。程序、资源和 locales 仍须成套替换；主程序仍为 x86，保留文件无需二进制补丁。企业微信自动选择旧组件的原因尚未确定，升级也可能覆盖替换结果。

[dll-overrides.txt](configs/dll-overrides.txt) 保存环境变量覆盖列表，另由 [cef-builtin.reg](configs/cef-builtin.reg) 为三个 CEF 程序选择内建 `vcruntime140_1`、`dbghelp`；`n,b` 表示优先原生实现、保留 Wine 内置回退，前提是配套 DLL 确实存在。[qt.conf](configs/qt.conf) 只放在版本目录的 `wemail/`，修改该组件的 DPI 感知模式即可，无需调整全局显示缩放。

## 字体与输入

| 层次 | 配置及作用 |
|---|---|
| 字体文件 | 新装用 winetricks 安装西文字体，另复制 4 个思源 CN OTF 并注册；[原清单](materials/fonts.json)保留历史 31 文件快照 |
| GDI | `FontSubstitutes` 映射中文字体名称；`FontLink\SystemLink` 为西文字体补充中文字形 |
| DirectWrite | Wine `Fonts/Replacements` 映射应用请求的字体族；仅配置 GDI 时，水印等位置仍可能缺字 |
| 编辑框 | x86 微软 `riched20.dll` 配合 `riched20=n,b`；字体探针通过后，替换控件才修复编辑中的中文方框 |
| 悬浮提示 | 将 `WindowMetrics\StatusFont` 设为 `Source Han Sans CN`；提示控件使用独立系统字体，主界面换字体不一定生效 |

字体注册和映射分别由 [fonts-install.reg](configs/fonts-install.reg)、[fonts-replacements.reg](configs/fonts-replacements.reg) 提供。它们使用思源字体替代所请求的中文字体族，不等于安装了微软原版微软雅黑、宋体。31 个文件是已验证集合，尚未做逐个移除试验。

RichEdit 的版本与 SHA256 见 [versions.json](materials/versions.json)。`winetricks riched30` 安装匹配的 `windows/syswow64/riched20.dll` 及配套 `msls31.dll`；该目录存放 x86 DLL。此次没有改动宿主输入法，也未定位 Wine RichEdit 内部的具体缺陷。

提示字体原为 Tahoma，部分中文回退失败，且 FontLink 可能随区域设置变化被重写。[status-font-probe](scripts/status-font-probe.c) 通过 Windows API 设置 StatusFont 的字体名，保留原字号与字重；不用手写 LOGFONT 二进制值。完整重启企业微信后再检查“截图”“查找聊天记录”等提示。

## 内嵌页面与邮件的 fcitx5 提交

邮件标题可以输入中文，正文有候选框却无法提交。诊断确认 fcitx5 的 XIM 已交付文字，但 Wine 把提交交给 WeMail 的外层 Qt 窗口，实际焦点在另一进程的 CEF 子窗口。公共版 Wine 11.19 和 10.14 都复现；关闭预编辑显示也无改善。正文使用 CEF，已有的 RichEdit、字体和 VC 组件不负责这条路径；winetricks 的 `msctf` 安装的是 XP x86 组件，未找到与此故障匹配的现成组件方案。

[ime-bridge.c](scripts/ime-bridge.c) 是针对已验证组合的 Linux x86_64 输入桥接模块。启动器只在企业微信启动链上加载 `ime-bridge.so`，模块只在 `WeMail.exe` 或 `WXWork.exe` 内处理 XIM 已提交文本：焦点必须是 `CefBrowserWindow` 下的 `Chrome_WidgetWin_1`，编辑器与最外层宿主属于不同进程，且输入事件的 X11 窗口必须与该宿主窗口匹配，才向当前编辑器投递 Unicode `WM_CHAR`。其他输入沿用 Wine 原路径。

文档正文通过多层 Qt 窗口嵌入企业微信主窗口，审批通过 `WXWorkWebViewHostWindow` 嵌入主窗口或独立业务窗口。因此模块同时支持 WeMail 和主程序作为输入宿主，使用实际 X11 窗口归属检查，覆盖独立邮件正文、文档正文和审批编辑框。

模块保留系统 `/usr/bin/wine`，无需修改系统 DLL、企业微信二进制或剪贴板。默认不记录提交过程；`WECOM_IME_DEBUG=1` 只记录数量和窗口句柄，不记录文字。通过 `WECOM_IME_FIX=0` 并完整重启可关闭。用户确认日常环境的邮件标题和正文、文档正文、审批输入框及聊天输入均能提交中文。邮件收发、长文本、复杂组合输入及长期稳定性仍未覆盖。

生产桥接逐个投递 UTF-16 `WM_CHAR`；若中途投递失败，已发送部分会保留，不能保证整段提交完整。常用输入验收不能证明 IME 已完整修复。[隔离 Wine 11.19 实验](experiments/wine-ime-commit/README.md)改为整段文本一次入队，再由接收线程向固定窗口分发 `WM_CHAR`，消除跨进程逐字入队的失败窗口。它仍非编辑器事务，也未接入标准 IMM 组合输入协议；尚未通过真实企业微信验收，未替换日常 Wine 环境。

它使用 Wine `win32u.so` 的内部 Unix 接口和 CEF 窗口类别；Wine 或企业微信升级后需要重新验证，不能视为上游 Wine 已修复。适用平台为 Arch x86_64 的新 WoW64 运行方式。模块路径不能含空白或冒号，启动器遇到这种路径会跳过并提示。

## CEF 映像内存共享

Wine 11.19 对未按宿主页对齐的 PE 原始段偏移，无法直接用文件映射共享，转为 `pread` 到每个进程自己的匿名内存。当前 x64 `libcef.dll` 使用 512 字节文件对齐；实测每个 CEF 进程为映像重复保留约 232 MiB 私有脏页。这不是每个 CEF 独占的大块内核内存，也不能通过仅限制 V8 堆来消除。

可选 [align-cef-memory.py](scripts/align-cef-memory.py) 只处理已知 SHA256，将文件布局重排为 4096 字节对齐，保留 RVA 与代码内容，并更新调试文件指针。它只生成新副本，拒绝未知源文件、原地修改和覆盖已有输出；独立输出哈希校验防止产生意外版本。厂商清单保持不变，`verify-materials.py --cef-layout page-aligned` 仅对这一个 DLL 使用单独指纹，其余文件仍按厂商校验。

两个探针强制逐页读入后共享效果仍存在；隔离完整应用的 CEF 私有脏页降至每进程约 15 MiB，用户确认文档正文和邮件页面正常。页面 renderer 数量与业务内容仍会影响总内存，不能把不同页面负载的应用 PSS 当作严格 A/B。原始签名会失效，更新及长期兼容性未验证，所以不自动修改应用、不改默认安装材料、不分发修改后的 DLL；升级后必须重新评估。证据、生成和回退方法见 [CEF 内存实验](experiments/cef-memory/README.md)。

文档预加载确实存在，但 `wedoc/prepare_enabled` 等目前只是二进制候选字符串，没有确认其配置来源及关闭效果。本次不写入猜测开关，也不通过停止整个 WeMail 来关闭预加载。

## Wine 页保护元数据

CEF/V8 预留 1 TiB `PAGE_NOACCESS` 地址时，Wine 11.19 的每页保护字节表需要约 256 MiB；旧清零路径将其全部写入物理内存。这与 CEF 映像复制是独立瓶颈，同样属于用户态开销。专用 [vprot 补丁](experiments/wine-vprot/wine-11.19.patch)只对独立匿名元数据中的完整宿主页用 `MADV_DONTNEED` 清零，边缘、非零值和失败路径仍用 `memset`，不改变 V8 地址区或沙箱。

底层同条件三轮 PSS 增量由每轮 262148 KiB 降至 11 / 8 / 8 KiB；内存语义、上游测试和隔离页面验收通过后，正式样本 31 个 Wine 进程、7 个 renderer 总 PSS 约 2.94 GiB。该总量不是匹配页面负载的 A/B；短时观察有轻微增长，不证明长期无泄漏。来源、哈希、可回退安装和测试边界见 [vprot 记录](experiments/wine-vprot/README.md)。

运行时只在用户目录保存三个已验证二进制，其他 Wine 模块链接同版本系统安装；不向 `/usr/lib/wine` 写入。loader 和 preloader 不能链接回系统，否则可能静默加载错误的 ntdll。启动器校验随安装分发的固定清单，而非信任候选自行声明哈希；系统升级后先退回原生 Wine，重新验证后才更新指纹。

## 启动等待

已观察到的五分钟等待来自 `drive_c/ProgramData/boost_interprocess/01000000/WXWORK-TBS-HELPER-LOCK`：文件长 12 字节，初始化状态为 0，网页组件共享内存初始化等待 **300003 ms**。移走失效锁后，该次主界面约 **4.4 秒**显示，用户确认聊天和文档正常。最初导致锁失效的原因尚未确定。

[prepare-startup.py](scripts/prepare-startup.py) 在启动前只处理同名、12 字节、状态 0 或 1 的普通文件。确认目标 prefix 无 Wine 进程后才移动到 `setup-backups/startup-lock-*`；活动状态不明、正常状态 2 或符号链接均不处理。脚本不清理聊天数据，也不自动删除 `CUR_LOGIN_UIN_KEY`；只移走后者的尝试没有解决等待。

主窗口可能在启动超过 120 秒后才出现，固定超时会让辅助程序提前结束。修复按目标 prefix 中的应用进程存活状态等待：标题栏通过 Wine 的 Toolhelp32 进程快照识别 `WXWork.exe`，托盘通过同用户 `/proc` 进程名、执行参数和 `WINEPREFIX` 识别。窗口隐藏或暂时缺失不代表进程退出。

最初 120 秒内若一直未确认目标进程存在，辅助程序仍结束等待，以约束启动失败后的闲置进程；观测未知也不会延长这段初始宽限，这不等于确认应用已退出。观察到目标进程后，可持续等待慢启动；连续确认进程不存在满 6 秒才结束，短暂进程交接可恢复等待，权限或枚举结果未知会清空连续缺失计时。标题栏绑定主窗口后监视已绑定的进程句柄，只更新同一 PID 的替换窗口，不跟随新的应用进程。Python 托盘助手接受主进程改名后的 `UIThread` 进程名，仍核对执行参数和 prefix。两个助手已部署，并通过模拟进程与时钟的回归测试及窗口助手的 32 位编译链接；真实慢启动尚未验收。测试命令见[仓库验证](verification-and-recovery.md#仓库验证)。

## 标题栏与边缘缩放

企业微信的标题栏和分层阴影是独立窗口。[titlebar-managed](scripts/titlebar-managed.c) 只匹配主进程及对应的窗口所属关系：

| 窗口 | 调整 | 解决的问题 |
|---|---|---|
| `TitleBarWindow` | 增加 `WS_SYSMENU` | 让标题栏纳入桌面窗口管理，随整窗退到其他应用之后 |
| 同上 | 清除 `WS_EX_NOACTIVATE` | 点击标题栏可以将整窗置前 |
| 主窗口所属的分层 `PerryShadowWnd` | 增加 `WS_EX_TRANSPARENT` | 阴影不再拦截鼠标，边缘拖动交给主窗口 |

原标题栏并未设置 TOPMOST；问题是它未受桌面窗口管理。最终标题栏扩展样式为 `0x00080080`。边缘缩放已从当时的最小尺寸 `986×650` 放大至 `1379×933`，达到最小尺寸时继续向内拖动无效。

辅助程序每秒检查样式，不周期性抢焦点或强制改变窗口尺寸。同一 prefix 仅运行一个 watcher，随主进程退出；开关和更新方法见[日常启动](verification-and-recovery.md#日常启动与开关)。

## 任务栏最小化恢复

在 Wine 11.19 / KDE Plasma 6.8.0 / 企业微信 5.0.11.6018 上，任务栏激活请求先让 X11 主窗口从 `Iconic` 变为 `Normal` 并置前，随后又收到 `WM_CHANGE_STATE = Iconic`，重新最小化。窗口线程仍能响应，直接投递 Windows 标准 `WM_SYSCOMMAND / SC_RESTORE` 能稳定恢复。Windows 前台事件探针确认：主窗口成为前台时，`IsIconic` 仍为真；具体导致 Wine 回写最小化的内部代码分支尚未锁定。

窗口助手复用已有的 `EVENT_SYSTEM_FOREGROUND` 回调，仅当事件目标是当前 prefix 已绑定的主窗口、它仍是当前前台、可见、启用且仍最小化时，异步投递一次 `SC_RESTORE`，让应用在自己的消息队列中完成恢复。其他窗口、过时的前台事件、隐藏到托盘、禁用窗口和已恢复窗口均跳过。该修复没有定时恢复或额外的常驻进程，与标题栏修复共用 `WECOM_TITLEBAR_FIX` 开关；命中时日志记录 `taskbar foreground restore posted=0x00000001`。

修复已部署；应用内与桌面两种最小化方式各验证 10 次，均能经任务栏等效请求恢复并保持前台。该测试未校验几何，不能据此断言位置和尺寸正常。最小化时右键菜单不显示由下述托盘菜单修复处理。验证细节及回退方法见[任务栏恢复修复](verification-and-recovery.md#任务栏最小化恢复)。

## 多屏托盘菜单与悬浮提示

KDE Wayland 的托盘点击坐标与 Wine/XWayland 使用的坐标原点、缩放不同。仅补固定横向偏移只能修复其中一块屏幕，因此采用逐屏换算。

[tray-geometry.py](scripts/tray-geometry.py) 在启动时读取 KScreen 和 XRandR，按输出名称匹配每块屏幕，生成平移与缩放映射。窗口辅助程序处理两类弹窗：企业微信主进程所属、由托盘触发的 `DuiMenuWnd` 菜单，以及 Wine Explorer 创建、属于当前唯一托盘图标的 `tooltips_class32` 悬浮提示。此前仅监听企业微信进程，遗漏了后者。

两类弹窗共用逐屏映射，核对托盘位置与布局后将窗口限制在目标工作区内。悬浮提示额外检查窗口归属及进程一致性，排除气泡通知；等待定位期间托盘位置改变则跳过。移动保持窗口大小和焦点，不改变应用内部提示的位置。

缺少依赖、输出无法匹配、来源坐标落入多个屏幕或运行时几何检查不符时，跳过修正。双屏 135% / 100% 的菜单及悬浮提示已确认正常；其他排列与旋转未覆盖。改变屏幕排列或缩放后，正常退出企业微信再启动以重新计算。

## 最小化时的托盘菜单

双屏布局中可复现：`DuiMenuWnd` 在 Win32 中可见、非最小化且尺寸为 116×97，但 X11 的 `WM_STATE` 为 `Iconic`。KWin 6.8 的 [x11window.cpp](https://github.com/KDE/kwin/blob/v6.8.0/src/x11window.cpp) 在管理附属窗口时，若其主窗口均不可见，会将它一同最小化。Wine 没有同步菜单的 `WS_MINIMIZE`，普通显示命令因而不能恢复它。

试验中强制设置 `WS_MINIMIZE` 再调用 `ShowWindow` 会恢复空的正常位置，导致菜单变成 0×0；补回尺寸又产生焦点变化。这两个方案均已撤销。当前方案保留 Win32 状态、owner 和主窗口：窗口助手仅针对刚由托盘打开、同进程、owner 为当前最小化主窗口的菜单，取得精确 X11 ID 和校正后的矩形，写入 prefix 的 `drive_c/windows/temp/wecom-tray-menu-request`。请求为 32 字节，仅含版本、窗口 ID、序号和矩形，不含聊天内容。

已有 Python 托盘助手用 inotify 监听写入完成，不读取启动前遗留请求、不忙轮询。它拒绝超过 1 秒、长度/版本错误、跨 prefix/PID、owner 不符、主窗口不再最小化或矩形越界的请求；菜单尚处于创建时的 Withdrawn/Normal 状态时，最多等 750 ms，避免错过 KWin 随后的自动最小化；通过后仅向目标菜单发送 `XMapWindow` 和带原尺寸的定位请求。定位必须在 map 后发送，否则 KWin 会恢复校正前的位置。正常菜单关闭仍走下述原有处理。

此功能需要 `WECOM_TITLEBAR_FIX` 和 `WECOM_TRAY_ICON_FIX` 两个助手均启用；`WECOM_TRAY_FIX=0` 仍仅关闭坐标换算。当前部署和实际验收情况见[最小化菜单修复](verification-and-recovery.md#最小化托盘菜单与二级菜单)。

## 托盘菜单关闭

实测托盘右键菜单显示后，前台仍可能停留在 Wine 托盘；即使临时激活菜单，再点击外部让它失焦，菜单仍不消失。正常 `WM_CLOSE` 消息可以让企业微信同时清理菜单及阴影窗口，因此仅修正坐标或焦点不足以恢复关闭行为。

窗口辅助程序只跟踪已确认由托盘打开的企业微信 `DuiMenuWnd`。真实事件记录确认：展开子菜单时，焦点会短暂落到其 `PerryShadowWnd` 阴影上。前台保护因此沿 owner 链检查同进程、可见的菜单和阴影，最多 16 层且必须回到当前托盘根菜单；主窗口阴影、别的菜单、其他进程和循环关系均不计入。检测到菜单范围外的鼠标按下，或前台切换到托盘/菜单之外时，重新核对进程和所属关系，再向该菜单发送 `WM_CLOSE`。菜单及子菜单内部的点击保留原行为，鼠标事件继续传递。此处理由 `WECOM_TITLEBAR_FIX` 控制，独立于 `WECOM_TRAY_FIX` 的坐标修正。

Wine 在焦点切到桌面时还可能将前台窗口清空，却不发出前台切换事件。为覆盖实测的遗漏，辅助程序仅在托盘菜单打开期间每 50 ms 检查前台状态；离开托盘/菜单组持续 100 ms 后关闭，菜单结束后恢复原等待间隔。

## 托盘消息提醒

捕获到 20×20 图标中的闪烁空白帧实际变成了居中的 16×16 不透明黑块，Wine 原始图像和 KDE 收到的像素一致。[tray-icon.py](scripts/tray-icon.py) 用 XDamage 监听重绘，只匹配这一已观察到的像素形状，将其改为空白占位；正常图标不变，消息提醒继续闪烁。

占位区保留 1/255 的透明度：视觉上为空白，同时避免 KDE 丢弃全透明更新，也保留托盘位置和原点击区域。程序通过 X11 进程信息确认当前 prefix，要求其中存在企业微信且仅有一个匹配的嵌入式 Wine 图标；尺寸、像素或归属不符时不修改。用户已确认空白占位效果。

## 托盘点击激活

托盘属于 Wine Explorer。左键点击虽然恢复了企业微信，但前台窗口可能仍是 `__wine_tray_icon`；实测直接 `SetForegroundWindow` 返回失败，临时连接前台输入队列后调用成功。只修改 X11 的 `WM_TAKE_FOCUS` 协议没有解决，已恢复原属性。

窗口辅助程序监听当前 prefix 内唯一托盘图标上的左键释放，稍后确认企业微信已可见、未最小化，且前台仍是该托盘窗口，再临时 `AttachThreadInput`、激活主窗口并立即分离。右键菜单、隐藏/最小化操作和用户已切换到其他窗口的情况不触发强制激活。
