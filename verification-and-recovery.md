# 使用与维护

本页对应 **企业微信 5.0.12.6018 / 系统 Wine 11.20**，只说明正式配置的使用、更新和回退。修复原因见[配置说明](configuration-rationale.md)，新装见[安装步骤](install-from-scratch.md)。命令在普通用户的 Bash 终端执行。

## 日常启动与开关

使用 `WeCom-Native-Wine.desktop`，或执行：

```bash
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
test -f "$WECOM_PREFIX/system.reg"
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

启动器从 prefix 内加载辅助文件，日常运行不依赖仓库。直接执行 `wine WXWork.exe` 会跳过兼容配置。

| 环境变量 | 设置为 0 时 |
|---|---|
| `WECOM_TITLEBAR_FIX` | 停用标题栏、边缘缩放、任务栏恢复、托盘菜单与点击激活、聊天文件菜单修复 |
| `WECOM_TRAY_FIX` | 仅停用托盘菜单和提示的坐标修正 |
| `WECOM_TRAY_ICON_FIX` | 停用托盘空白帧及最小化菜单的原生显示 |
| `WECOM_IME_FIX` | 停用邮件及内嵌页面的 XIM 提交桥接 |
| `WECOM_VPROT_FIX` | 停用专用 Wine 运行时，使用系统 Wine |

开关在启动时读取，需先完整退出再设置。改变屏幕排列或缩放后也需重启，以重新计算托盘坐标。例如：

```bash
env WECOM_TRAY_FIX=0 bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

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
cp -- "$WECOM_SETUP/configs/dll-overrides.txt" "$WECOM_SETUP/configs/wine-vprot-runtime.sha256" \
  "$WECOM_PREFIX/wecom-compat/configs/"
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

源码、EXE、SO、脚本和覆盖列表配套更新；不要覆盖仍由进程映射的 EXE 或 SO。更新后按下面清单验收。恢复旧辅助版本时，停止目标 prefix，再从 `setup-backups/compat-update-*/wecom-compat/` 还原。

## 配置边界

正式部署使用原位升级，空 prefix 按安装文档配置。材料一致性不等于所有业务功能都已覆盖；实际邮件收发、审批提交、会议、复杂输入、长期稳定性和其他屏幕布局需结合用途检查。CEF 内建 dbghelp 的配置不保证所有崩溃报告流程兼容。

## CEF 映像内存共享

正式 CEF 为 **30 文件 + 页对齐**。仅 libcef.dll 使用[页对齐指纹](materials/cef64-page-aligned.json)，其他文件仍匹配厂商清单。安装工具保留源文件并生成新副本，日常启动不改写 DLL。

回退时停止目标 prefix，将安装前保存的厂商 libcef.dll 恢复到对应版本的 `compatible_web/`，再以 `--cef-layout vendor` 校验。新装步骤将原件保存为 `setup-backups/libcef.vendor.dll`；已有安装须以自身备份为准。页对齐版签名失效，升级时不可对未知 DLL 强行套用旧指纹。

## Wine 页保护元数据

当前系统 Wine 11.20 不加载专用 11.19 vprot 运行时。[固定清单](configs/wine-vprot-runtime.sha256)同时约束系统版本、依赖和候选文件，防止混用不兼容的 Wine 模块。

运行时选择保留 `WECOM_VPROT_RUNTIME` 绝对路径配置；默认位于用户数据目录的 `wecom-runtimes/wine-11.19-vprot-e38a364b`。仅当系统版本和全部指纹匹配时使用，缺失或不匹配则回退系统 Wine。需要停用时，完整退出后设置 `WECOM_VPROT_FIX=0`；持续停用可把该变量加入桌面入口的 `Exec=env ...`。

专用运行时的[正式源码补丁](patches/wine-11.19-vprot.patch)和[安装器](scripts/install-wine-vprot-runtime.py)保留版本约束。安装器只接受固定指纹的本地产物，输出必须是新目录，不修改系统 Wine 或 prefix。Wine 运行期间不要升级其共享依赖。

## 启动路径与慢启动等待

启动器将 prefix 解析为绝对物理路径。窗口助手依据目标应用进程存活状态等待，避免主窗口较晚出现时修复失效；等待边界见[配置原因](configuration-rationale.md#启动等待)。已有安装通过上面的辅助文件更新步骤成套同步。

## 任务栏最小化恢复

任务栏恢复使用窗口助手的前台事件回调，仅恢复当前绑定且仍最小化的主窗口。由 `WECOM_TITLEBAR_FIX` 控制，正常最小化和托盘隐藏不触发自动弹回。

## 最小化托盘菜单与二级菜单

`titlebar-managed.c/.exe` 与 `tray-icon.py` 成套工作，在主窗口最小化时显示并定位托盘菜单。子菜单及其阴影按同一菜单组处理，点击外部关闭根菜单，保留主窗口最小化状态。

此功能要求 `WECOM_TITLEBAR_FIX` 和 `WECOM_TRAY_ICON_FIX` 均启用；`WECOM_TRAY_FIX=0` 只关闭坐标换算。回退时恢复同一备份中的整套辅助文件，避免不同版本的请求格式或菜单逻辑混用。

## 聊天文件菜单与原生表格打开

窗口助手转发聊天菜单点击，避免菜单因焦点切换而提前销毁，支持另存为、文件及文字复制。菜单按下时销毁后会消耗配对释放，避免误点新对话框。该功能随 `WECOM_TITLEBAR_FIX` 控制。

xls/xlsx 通过 [native-spreadsheet-open.reg](configs/native-spreadsheet-open.reg)交给宿主默认应用，正式配置使用 `wps-office-et.desktop`。新装命令见[表格关联](install-from-scratch.md#9-配置表格文件打开)。

已有 prefix 导入前先备份 `HKCR\.xls`、`HKCR\.xlsx`、`HKCR\WeCom.NativeSpreadsheet` 和宿主原默认程序。该关联按需导入，启动器不会覆盖已有选择。回退时恢复原值；原本不存在的键应移除新增项，宿主默认应用单独恢复。

## 使用检查

| 功能 | 预期结果 |
|---|---|
| 启动、登录、聊天 | 中文界面及主窗口正常，切换聊天可用 |
| 文档与邮件页面 | 列表和正文正常加载，页面可以操作 |
| 中文显示与输入 | 常用输入框可选词、编辑和删除中文，提示无方框 |
| 标题栏与任务栏 | 点击置前、拖动、边缘缩放和最小化恢复正常 |
| 托盘 | 各屏幕菜单及提示靠近图标，子菜单保持，外部点击关闭；主窗口最小化时仍可打开菜单 |
| 消息提醒 | 正常图标与空白占位交替，左键恢复并激活主窗口 |
| 聊天文件 | 另存为、复制、打开和普通文字复制可用，表格由 WPS 打开 |

只检查输入时无需发送消息、提交审批或保存无关修改。安装材料可用以下只读命令核对：

```bash
export WECOM_SETUP="/path/to/archlinux-wine-wecom-setup"
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
export WECOM_VERSION_DIR="$WECOM_PREFIX/drive_c/Program Files (x86)/WXWork/5.0.12.6018"
python3 "$WECOM_SETUP/scripts/verify-materials.py" \
  --installer "$WECOM_SETUP/assets/WeCom_5.0.12.6018.exe" \
  --cef-profile wine-builtin --cef-layout page-aligned --cef-dir "$WECOM_VERSION_DIR/compatible_web" \
  --richedit "$WECOM_PREFIX/drive_c/windows/syswow64/riched20.dll" \
  --fonts-profile cn --fonts-dir "$WECOM_PREFIX/drive_c/windows/Fonts"
```

## 重启与升级

保存工作后只停止目标 prefix：

```bash
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom"
test -f "$WECOM_PREFIX/system.reg"
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -k
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -w
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

### 应用版本与兼容配置

新装、升级及正式修复统一使用 [versions.json](materials/versions.json)，当前为企业微信 **5.0.12.6018**。[配置清单](materials/app-upgrade.json)保存主程序、WeMail 和 CEF 指纹。新版本应重新确认兼容组件，不强制沿用不匹配的 CEF。

1. 核对安装包版本及 SHA256，保存工作并退出目标 prefix。
2. 在 prefix 外备份完整环境、实际存放聊天数据的 Documents/WXWork 及桌面入口。Documents 若为宿主目录链接，只复制 prefix 不会包含链接目标。
3. 在目标 `WINEPREFIX` 中运行安装包 `/S`。安装器可能清理旧版本目录，兼容配置应从备份读取。
4. 保留新版原始网页组件，在新版本目录应用成套 x64 CEF、页对齐和 WeMail Qt DPI 配置。保留字体、RichEdit、输入桥接、窗口助手和 WPS 关联。
5. 核对组件与配置，通过原桌面入口启动并执行使用检查；在同一提交中更新默认版本、指纹和 setup 文档。

完整回退应恢复应用、注册表及配套账号数据；先保存升级后产生的数据，不能假定旧程序可以读取新版数据库。组件原件备份不等于账号数据的完整备份。

## 单项回退

先停止目标 prefix，并核对备份存在且属于对应版本：

| 配置 | 回退方法 |
|---|---|
| CEF 页对齐 | 恢复厂商 libcef.dll，以 vendor 布局校验 |
| 成套 CEF | 保存当前目录并恢复原组件目录；原 x86 页面兼容问题可能重新出现 |
| Qt DPI | 恢复原 wemail/qt.conf；原本不存在时移走新增文件 |
| DLL 覆盖 | 按安装前值恢复对应项，保留其他覆盖 |
| RichEdit | 恢复原 riched20.dll、msls31.dll 及覆盖值；只有原本缺失时才移走新增文件 |
| StatusFont | 从 window-metrics.reg 恢复原 StatusFont 值，保留其他窗口参数 |
| 字体映射 | 逐项恢复安装前值；reg 导入只合并，不会自动删除新增键 |
| 窗口、托盘、输入桥接 | 使用启动开关停用，或恢复同次备份中的整套辅助文件 |
| 表格关联 | 恢复 Wine 原关联和宿主原默认程序 |
| 专用 Wine | 完整退出后设置 WECOM_VPROT_FIX=0 |

## 辅助文件维护

| 文件 | 职责 |
|---|---|
| launch-wecom.sh | 固定启动环境及加载顺序 |
| prepare-startup.py | 在空闲 prefix 中备份已识别的失效共享锁 |
| tray-geometry.py | 计算逐屏坐标映射 |
| tray-icon.py | 修复托盘空白帧及最小化菜单的原生显示 |
| titlebar-managed.c / .exe | 维护标题栏、任务栏、托盘菜单及聊天文件菜单 |
| set-status-font.c / .exe | 设置提示字体，保留其他窗口参数 |
| ime-bridge.c / .so | 转交嵌入式 CEF 编辑器的跨进程 XIM 提交 |
| verify-materials.py | 只读校验正式材料 |
| align-cef-memory.py | 为已知 CEF 原件生成页对齐副本 |
| install-wine-vprot-runtime.py | 安装固定指纹的专用运行时 |
| package-cef-example.py | 按组件清单生成本地归档 |

辅助文件安装到 prefix 的 `wecom-compat/`。重建输入桥接需要 GCC 和 X11 开发头文件；两个 EXE 需要 clang、lld 和 Wine 开发导入库。在仓库根目录执行：

```bash
gcc -std=c11 -shared -fPIC -Wall -Wextra -Werror -O2 \
  scripts/ime-bridge.c -o /tmp/wecom-ime-bridge.so -ldl
cp /tmp/wecom-ime-bridge.so scripts/ime-bridge.so

clang --target=i686-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -Wall -Wextra -Werror -c scripts/set-status-font.c -o /tmp/wecom-set-status-font.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
  /out:scripts/set-status-font.exe /tmp/wecom-set-status-font.obj \
  /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a

clang --target=i686-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -Wall -Wextra -Wno-unused-parameter \
  -c scripts/titlebar-managed.c -o /tmp/wecom-titlebar-managed.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
  /out:scripts/titlebar-managed.exe /tmp/wecom-titlebar-managed.obj \
  /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a
```

重建后通过辅助文件更新步骤同步到已停止的环境，再检查实际使用行为。
