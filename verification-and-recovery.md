# 使用与维护

从空 prefix 安装见[安装步骤](install-from-scratch.md)，配置原因见[配置说明](configuration-rationale.md)。以下命令在普通用户的 Bash 终端执行，路径按实际环境调整。

## 日常启动与开关

打开 `WeCom-Native-Wine.desktop`，或从终端运行：

```bash
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
test -f "$WECOM_PREFIX/system.reg"
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

启动器从 prefix 内加载配套文件，日常运行不依赖仓库。直接执行 `wine WXWork.exe` 会跳过这些检查与窗口修复。

| 环境变量 | 设置为 `0` 时 |
|---|---|
| `WECOM_TITLEBAR_FIX` | 停用标题栏、边缘缩放、托盘坐标、菜单关闭和点击激活修复；保留启动前锁检查与图标修复 |
| `WECOM_TRAY_FIX` | 只停用托盘菜单和悬浮提示的坐标修正 |
| `WECOM_TRAY_ICON_FIX` | 只停用托盘黑块的空白占位修正 |
| `WECOM_IME_FIX` | 停用邮件及内嵌页面的 XIM 提交桥接；恢复系统 Wine 的输入行为 |

开关在启动时读取；先完整退出企业微信，再设置开关启动。例如：

```bash
env WECOM_TRAY_FIX=0 bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

改变屏幕排列或缩放后，也需退出再启动以重新计算。仅最小化或重复点击桌面入口不会刷新已运行的辅助程序。

## 更新启动辅助文件

先保存工作并退出目标企业微信。以下命令备份原辅助文件，再同步仓库版本，不重装应用、字体或 CEF：

```bash
set -euo pipefail
export WECOM_SETUP="/path/to/archlinux-wine-wecom-setup"
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
test -f "$WECOM_PREFIX/system.reg"
test -d "$WECOM_PREFIX/wecom-compat/scripts"
test -f "$WECOM_SETUP/scripts/titlebar-managed.exe"

env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -k
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -w
mkdir -p -- "$WECOM_PREFIX/setup-backups"
compat_backup="$(mktemp -d "$WECOM_PREFIX/setup-backups/compat-update-XXXXXXXX")"
cp -a -- "$WECOM_PREFIX/wecom-compat" "$compat_backup/"
cp -- "$WECOM_SETUP/scripts/launch-wecom.sh" "$WECOM_SETUP/scripts/prepare-startup.py" \
  "$WECOM_SETUP/scripts/tray-geometry.py" "$WECOM_SETUP/scripts/tray-icon.py" \
  "$WECOM_SETUP/scripts/titlebar-managed.c" \
  "$WECOM_SETUP/scripts/ime-bridge.c" "$WECOM_SETUP/scripts/ime-bridge.so" \
  "$WECOM_SETUP/scripts/titlebar-managed.exe" "$WECOM_PREFIX/wecom-compat/scripts/"
cp -- "$WECOM_SETUP/configs/dll-overrides.txt" "$WECOM_PREFIX/wecom-compat/configs/"
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

源码、EXE、SO、脚本和覆盖列表配套更新；不要覆盖仍由进程映射的 EXE 或 SO。更新后按下面清单验收。恢复旧辅助版本时，停止目标 prefix，再从 `setup-backups/compat-update-*/wecom-compat/` 还原。

## 验证范围

截至 **2026-10-12**，验证结果如下。不同轮次的验收范围分别记录，不代表每次 DLL 变更都重新检查了全部功能。

| 范围 | 验证结果与证据 |
|---|---|
| 此前 78 文件 CEF + winetricks | 独立 prefix 与日常环境重启后，邮件、文档、工作台页面、中文输入和显示正常；实际模块路径已核对。见[组件验证](materials/winetricks.md#独立验证与精简范围)及[日常迁移](materials/winetricks.md#日常环境迁移) |
| fcitx5 输入桥接 | 日常环境聊天、邮件标题和正文、文档正文、审批输入框均能提交中文；主程序与 WeMail 的模块加载已核对，日常诊断日志关闭 |
| 30 文件 CEF + winecfg | 独立 prefix 逐步替换 `vcruntime140_1`、46 个 API-set 和 `dbghelp`，三轮独立重启后页面和中文输入均正常；正式环境也已切换，用户确认页面、中文输入和显示正常，见[部署记录](materials/winecfg-deployment.json)。见[试验记录](materials/winetricks.md#winecfg-覆盖试验) |
| 启动与窗口 | 启动锁修复后一次主界面显示约 4.4 秒；标题栏遮挡、激活、连续拖动和边缘缩放已验收。间歇拖动失败未再次复现，尚未定位独立原因 |
| 双屏托盘 | 两屏菜单／提示位置、点击外部关闭、菜单内部操作、消息空白帧、左键置前及最小化恢复均已验收；菜单关闭另有焦点探针证据 |

未验收：邮件收发、审批提交、长文本和复杂组合输入、会议与其他插件、长期稳定性、整理后的完整从零安装、宿主重启及其他屏幕排列／旋转。30 文件试验未测试实际崩溃转储和报告流程，也未重新完整验收窗口和托盘交互。

此前的临时 prefix、诊断文件、归档及历史回退备份已清理。本次 30 文件正式切换另保留 `setup-backups/cef-builtin-<时间>/`：包含 48 个移出文件、原覆盖列表、桌面入口及仅恢复本次六个注册表值的 `rollback.reg`，不包含账号快照。此次主窗口出现较晚，窗口／托盘辅助程序等待超时后已补启动；延迟原因尚未确定。

`materials/versions.json` 保留首次安装时的 Wine 11.18 及材料版本，不代表当前运行状态；后续启动和窗口修复使用 Wine 11.19。材料一致性、脚本静态检查和用户实际操作是不同层次的验证。

## 验收清单

| 检查 | 通过条件 |
|---|---|
| 启动和登录 | 中文界面正常，登录后及时出现主界面 |
| 文档 | 能看到列表，点击正文后可以加载并操作 |
| 中文输入 | 编辑、删除中文和标点正常，无需发送测试消息 |
| 邮件中文输入 | 标题、正文分别输入并选词，连续输入不丢字、不重复；再确认聊天输入正常，测试内容不要发送 |
| 文档和审批中文输入 | 在草稿文档正文、审批说明框输入并选词，不丢字、不重复；不要提交审批或保存测试修改 |
| 悬浮提示 | “截图”“图片”“文档”“文件”“查找聊天记录”无方框 |
| 标题栏 | 单击能置前，切换其他应用时整体退后，拖动和最小化/恢复正常，无重复任务栏项 |
| 边缘缩放 | 左右和底部向外拖动可放大，向内缩小遵守最小尺寸限制 |
| 托盘菜单 | 在各屏幕分别关闭、重开，菜单靠近图标；点击其他应用、桌面或主窗口空白处后关闭，菜单内部悬停和点击正常；应用内部菜单正常 |
| 托盘悬浮提示 | 在各屏幕悬停图标，提示靠近图标且留在屏幕内；移开鼠标后正常消失 |
| 托盘消息提醒 | 来消息后正常图标与空白占位交替，无黑块；左键点击能激活主窗口 |
| 其他功能 | 按实际用途检查工作台和邮件 |

材料/提示字体诊断可使用下面的只读检查；`status-font-probe.exe` 不带参数只检查，加 `-set` 才保存设置：

```bash
export WECOM_SETUP="/path/to/archlinux-wine-wecom-setup"
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
export WECOM_VERSION_DIR="$WECOM_PREFIX/drive_c/Program Files (x86)/WXWork/5.0.11.6018"
test -f "$WECOM_PREFIX/system.reg"
env -u LC_ALL WINEPREFIX="$WECOM_PREFIX" WINEDEBUG=-all \
  LANG=zh_CN.UTF-8 LC_CTYPE=zh_CN.UTF-8 LC_MESSAGES=zh_CN.UTF-8 \
  WINEDLLOVERRIDES=winemenubuilder.exe=d \
  /usr/bin/wine "$WECOM_SETUP/scripts/status-font-probe.exe"
python3 "$WECOM_SETUP/scripts/verify-materials.py" \
  --installer "$WECOM_SETUP/assets/WeCom_5.0.11.6018.exe" \
  --cef-profile wine-builtin --cef-dir "$WECOM_VERSION_DIR/compatible_web" \
  --richedit "$WECOM_PREFIX/drive_c/windows/syswow64/riched20.dll" \
  --fonts-profile cn --fonts-dir "$WECOM_PREFIX/drive_c/windows/Fonts"
```

## 按现象定位

| 现象 | 优先检查 |
|---|---|
| 登录后恰好等待约五分钟 | [Boost 初始化锁](configuration-rationale.md#启动等待)及 `wecom-startup.log`；不要自动删除 `CUR_LOGIN_UIN_KEY` |
| 正文白屏、renderer 退出 | [实际加载的 CEF 版本和架构](configuration-rationale.md#文档页面)、新增 Crashpad 转储 |
| 工作台可用，但文档列表空白 | WeMail 的 `wemail/qt.conf`、Qt 感知模式和父窗口；renderer 存活不等于嵌入正常 |
| MSVCP140 缺失导入 | 配套 DLL 是否存在，启动器的覆盖列表和实际加载路径 |
| 水印或正文方框 | [字体文件、GDI 与 DirectWrite 映射](configuration-rationale.md#字体与输入)；仅看 Linux `fc-match` 不够 |
| 只有编辑框方框 | x86 RichEdit 校验及 native 加载情况 |
| 邮件/文档/审批有候选框但选字后不出字 | `ime-bridge.so` 是否存在且由启动器加载、`WECOM_IME_FIX` 是否开启；不是依靠重装字体解决 |
| 只有悬浮提示方框 | StatusFont 是否生效，设置后是否完整重启 |
| 标题栏盖在最前或点击不激活 | [窗口辅助程序](configuration-rationale.md#标题栏与边缘缩放)是否运行、标题栏样式是否已调整 |
| 边缘不能拖动 | 阴影鼠标穿透是否生效；先向外放大，当前布局最小尺寸为 `986×650` |
| 托盘菜单或悬浮提示偏移 | [逐屏坐标映射](configuration-rationale.md#多屏托盘菜单与悬浮提示)与 `wecom-titlebar.log`；分别查看 `tray menu` 或 `tray tooltip coordinate correction`，布局改变后重新启动 |
| 托盘菜单点击外部不消失 | [菜单关闭处理](configuration-rationale.md#托盘菜单关闭)及日志中的 `tray menu dismiss`；核对触发原因和 `posted` 结果 |
| 托盘消息提醒闪黑块 | [空白占位辅助程序](configuration-rationale.md#托盘消息提醒)及 `wecom-tray-icon.log`；确认图标尺寸和像素仍匹配已验证情况 |
| 托盘左键只恢复、不置前 | [激活权限处理](configuration-rationale.md#托盘点击激活)及 `wecom-titlebar.log` 中的 `tray left-click activation` 结果 |

日志路径均相对于目标 prefix；`<用户名>` 按实际环境替换：

| 路径 | 用途 |
|---|---|
| `wecom-startup.log` | 失效锁检查、跳过或备份结果 |
| `wecom-titlebar.log` | 窗口样式、托盘监听及坐标修正 |
| `wecom-tray-icon.log` | 托盘图标匹配与黑块替换 |
| `drive_c/users/<用户名>/AppData/Roaming/Tencent/WXWork/Log/` | 应用日志，部分加密；`CEF/` 下为网页组件日志 |
| `drive_c/users/<用户名>/AppData/Local/wxworkweb/User Data/Crashpad/reports/` | CEF 崩溃转储 |

比较操作前后的新增日志/转储，避免把历史故障当成本次结果。普通启动保持 `WINEDEBUG=-all`，原始诊断文件留在本机。

## 重启与升级

保存未完成工作后，只停止目标 prefix，不使用 `killall wine` 或 `pkill wineserver`：

```bash
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
test -f "$WECOM_PREFIX/system.reg"
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -k
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -w
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

升级前停止目标 prefix 并备份；组件备份不等于聊天数据和账号状态的完整备份。升级后检查新版本目录的 `compatible_web`、`wemail/qt.conf`、VC/UCRT DLL，以及可能被 Wine 重建的 RichEdit、字体注册和 StatusFont，再执行完整验收清单。

未来版本是否仍需要各项修复须重新验证；不要强制沿用旧 CEF，或修改校验清单来迎合来源不明的文件。启动和停止目标 prefix 均使用系统 `/usr/bin/wine`、`/usr/bin/wineserver`。

## 单项回退

以下方法要求对应备份实际存在。此前本机的历史备份已清理，不能直接使用旧路径恢复；本次 `cef-builtin-*` 备份另行保留。新安装或后续变更应先建立自己的备份。先停止目标 prefix，再恢复对应项目：

| 配置 | 方法 |
|---|---|
| 30 文件 CEF → 原 78 文件 | 保存工作并停止目标 prefix，将本次 `cef-builtin-*/cef/` 的 48 个文件复制回 `compatible_web/`，恢复备份的 `dll-overrides.txt`，导入同目录 `rollback.reg`，以 `--cef-profile winetricks` 校验后重启；必须成套恢复文件与覆盖配置 |
| CEF | 改名保存当前 `compatible_web`，将 `compatible_web.x86-original` 恢复原名；原 x86 白屏可能复现 |
| Qt DPI | 保存并移走新增 `wemail/qt.conf` |
| VC/UCRT | 备份后只移除对应 DLL 覆盖项，保留其他配置 |
| RichEdit | 将 `setup-backups/riched20.dll` 放回 `windows/syswow64/`，移除启动器的 `riched20=n,b`；若用过 `winetricks riched30`，同时恢复其设置的 `HKCU\Software\Wine\DllOverrides\riched20`，还原备份的 `msls31.dll`（有 `msls31.was-absent` 标记时才移走新增文件） |
| StatusFont | 从 `setup-backups/window-metrics.reg` 恢复原 `StatusFont` 二进制值，避免覆盖其他窗口参数 |
| 字体映射 | 根据安装前导出值逐项还原；`.reg` 导入只合并，不会自动删除新增键值 |
| 窗口/托盘修复 | 使用[启动开关](#日常启动与开关)停用，或还原更新前的整套辅助文件；图标占位可单独用 `WECOM_TRAY_ICON_FIX=0` 停用 |
| 邮件/文档/审批输入桥接 | 设置 `WECOM_IME_FIX=0` 后完整重启，或停止后恢复配套辅助文件备份 |

备份命名可能与新装步骤不同，恢复前需核对备份内容与目标路径。

## 辅助文件维护

| 文件 | 职责 |
|---|---|
| `launch-wecom.sh` | 固定启动环境并按顺序运行辅助检查 |
| `prepare-startup.py` | 仅在空闲 prefix 中备份已识别的失效共享锁 |
| `tray-geometry.py` | 启动时计算 KDE Wayland/XWayland 逐屏映射 |
| `tray-icon.py` | 监听当前 prefix 托盘的重绘，将已识别的黑块帧改为空白占位 |
| `titlebar-managed.c` / `.exe` | 常驻维护标题栏、阴影输入、托盘菜单/提示坐标、菜单关闭和左键激活 |
| `status-font-probe.c` / `.exe` | 安装或诊断时读取/设置提示字体，不常驻 |
| `ime-bridge.c` / `.so` | 转交 WeMail 或主程序所承载的跨进程 CEF 编辑器的 XIM 提交，随企业微信启动链加载 |
| `verify-materials.py` | 只读校验安装包、组件和字体材料 |
| `package-cef-example.py` | 按厂商清单打包；默认完整 89 文件，`--cef-profile winetricks` 打包旧 78 文件，`--cef-profile wine-builtin` 打包当前 30 文件 |
| [DLL 加载探针源码](examples/dll-overrides/loader-probe.c) | 独立验证 winecfg 加载顺序与 manifest 重定向；不随日常启动运行 |

启动器、启动检查、两个托盘脚本和窗口辅助程序安装到 prefix 的 `wecom-compat/scripts/`，DLL 覆盖列表在 `wecom-compat/configs/`。其余配置/清单属于已验证材料快照；注册表文件使用 UTF-16，交给 Wine `reg.exe import`。

输入桥接模块使用 GCC 和 X11 开发头文件编译，不链接或替换 Wine 库。在仓库根目录执行：

```bash
gcc -std=c11 -shared -fPIC -Wall -Wextra -Werror -O2 \
  scripts/ime-bridge.c -o /tmp/wecom-ime-bridge.so -ldl
cp /tmp/wecom-ime-bridge.so scripts/ime-bridge.so
```

两个 EXE 均为附源码的小工具。重建需要 clang、lld 和 Wine 开发导入库，在仓库根目录执行：

```bash
clang --target=i686-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -c scripts/status-font-probe.c -o /tmp/wecom-status-font-probe.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
  /out:scripts/status-font-probe.exe /tmp/wecom-status-font-probe.obj \
  /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a \
  /usr/lib/wine/i386-windows/libgdi32.a

clang --target=i686-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -Wall -Wextra -Wno-unused-parameter \
  -c scripts/titlebar-managed.c -o /tmp/wecom-titlebar-managed.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
  /out:scripts/titlebar-managed.exe /tmp/wecom-titlebar-managed.obj \
  /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a
```

重建后按[更新步骤](#更新启动辅助文件)同步到已停止的环境，再验收实际行为。
