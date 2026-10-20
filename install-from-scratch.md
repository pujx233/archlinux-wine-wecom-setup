# 从零安装原生 Wine 企业微信

本页安装最新正常使用版本 **企业微信 5.0.12.6018 / 系统 Wine 11.20 / x64 CEF 5.0.79.5483**，与日常升级使用同一套正式修复。安装包及默认校验以 [versions.json](materials/versions.json) 为准，更新时同步本页与[应用配置](verification-and-recovery.md#应用版本与兼容配置)。

本页用于**空 prefix 的安装**。已有环境只需更新辅助脚本时，使用[维护步骤](verification-and-recovery.md#更新启动辅助文件)，不要重新执行字体注册、DLL 替换或 prefix 初始化。以下新装示例使用独立的 `WeCom-rebuild` 路径，避免覆盖当前安装。

本页采用 **30 文件 CEF + 页对齐 + winetricks + Wine 内建覆盖** 方案，并配置字体、输入及窗口助手和表格文件关联。30 文件清单必须配合第 5 节的应用专用注册表配置。安装完成后按[使用检查](verification-and-recovery.md#使用检查)逐项确认。

新装失败且尚未登录、没有用户数据时，可换一个空 prefix 重新安装。本页保存的原始组件和注册表备份供安装成功后的维护使用；已投入使用的环境需要撤销配置时，按[单项回退](verification-and-recovery.md#单项回退)处理。

先在仓库根目录打开 Bash。所有命令在普通用户的 **同一个 Bash 终端** 中分节执行；每节成功后再继续。不要使用 `sudo wine`。材料路径可修改，修改后重新运行材料校验。

## 1. 准备材料

准备系统 `/usr/bin/wine` 和 `/usr/bin/wineserver`，确认能运行 32/64 位程序；另需 Python 3、Bash、winetricks 及其下载/解包依赖、已生成的 `zh_CN.UTF-8` locale。示例中的 locale 检查使用 `rg`。滚动更新的软件包安装结果不等同于固定 Wine 版本，执行前应记录 `wine --version`。

KDE Wayland 下的托盘菜单与悬浮提示坐标修正另需 `kscreen-doctor`、`libX11` 和 `libXrandr`；托盘图标及最小化菜单助手另用 `libXdamage`，并通过 Linux inotify 接收窗口助手的菜单请求。缺少相关依赖或不在 KDE Wayland 环境时，跳过对应修复。

复用本机材料时，表中的 `drive_c/` 路径均相对于当前原生 Wine prefix `~/Works/Softwares/wine/WeCom`。这些文件已按仓库清单核对，可复制到 `assets/` 留存。

| 材料 | 本机已验证位置 | 需要留存的内容 |
|---|---|---|
| 安装程序 | `/home/<user>/Works/Download/WeCom_5.0.12.6018.exe` | 完整 EXE；SHA256 见 `materials/versions.json` |
| x64 CEF | 从匹配的腾讯客户端组件或已验证环境提取，按[本地准备步骤](examples/README.md#在本地准备-cef-示例素材)生成 | 30 个清单文件，第 5 节用 winetricks 提供另 11 个运行库，不只替换 `libcef.dll` |
| x86 RichEdit | 第 7 节通过 `winetricks riched30` 安装 | Microsoft 5.30.23.1200；不需要事先下载单独 DLL |
| 4 个中文字体 | Adobe 官方 Source Han Sans / Serif CN OTF，获取链接见[字体说明](materials/winetricks.md#字体安装) | Regular/Bold 共 4 个；西文字体在第 6 节用 winetricks 安装 |

CEF 是企业微信自行下载的腾讯组件；本项目只提供获取说明和本地打包工具，不上传第三方二进制。精简归档按原厂商清单保留 30 个文件，不修改二进制；安装时再应用页对齐。RichEdit 和字体采用下述组件组合，选择原因见[运行库说明](materials/winetricks.md#cef-运行库配置)。安装完成后，运行只依赖目标 prefix 内的文件。

将已取得的材料整理为以下结构，或在下方变量中填写自己的材料路径。`assets/` 已被 Git 忽略，不会随正常提交上传：

```text
assets/
├── WeCom_5.0.12.6018.exe
├── cef/                    30 文件的 x64 CEF 精简包
└── fonts/                  4 个思源 CN OTF
```

`WECOM_FONTS_SRC` 留空时同样按清单从仓库 `assets/fonts/` 读取。

```bash
bash
set -euo pipefail

export WECOM_SETUP="$PWD"
export WECOM_PREFIX="$HOME/Works/Softwares/wine/WeCom-rebuild"
export WECOM_INSTALLER="$WECOM_SETUP/assets/WeCom_5.0.12.6018.exe"
export WECOM_CEF_SRC="$WECOM_SETUP/assets/cef"
export WECOM_FONTS_SRC="$WECOM_SETUP/assets/fonts"

/usr/bin/wine --version
locale -a | rg -i '^zh_CN\.(utf8|UTF-8)$'

python3 "$WECOM_SETUP/scripts/verify-materials.py" --only installer --installer "$WECOM_INSTALLER"
python3 "$WECOM_SETUP/scripts/verify-materials.py" --only cef --cef-profile wine-builtin --cef-dir "$WECOM_CEF_SRC"
python3 "$WECOM_SETUP/scripts/verify-materials.py" --only fonts --fonts-profile cn --fonts-dir "$WECOM_FONTS_SRC"
```

预期分别输出 `PASS`，覆盖安装程序 SHA256、30 个 CEF 文件的清单 MD5 和 x64 PE 架构，以及 4 个 CN 字体 SHA256。RichEdit 在第 7 节安装后核对。清单 MD5 用于复核已知材料的一致性，不能替代来源核验。旧 78 文件方案用 `--cef-profile winetricks`、原厂 89 文件用 `--cef-profile full` 校验，原 31 字体快照用 `--fonts-profile snapshot` 校验；两套材料使用各自的校验模式。

## 2. 初始化独立 64 位 prefix

初始化前固定中文区域设置，避免 Wine 先按英文代码页生成默认字体配置。32 位主程序与 64 位 CEF 可以在同一 `win64` prefix 运行；`win32` prefix 不适合这个方案。

```bash
unset LC_ALL WINESERVER WINELOADER WINEDLLPATH
export LANG=zh_CN.UTF-8 LC_CTYPE=zh_CN.UTF-8 LC_MESSAGES=zh_CN.UTF-8
export WINEPREFIX="$WECOM_PREFIX" WINEARCH=win64 WINEDEBUG=-all
export WINEDLLOVERRIDES='winemenubuilder.exe=d'

if [[ -e "$WECOM_PREFIX" ]]; then
  printf '目标已存在，请换一个空路径：%s\n' "$WECOM_PREFIX"
  exit 1
fi
mkdir -p -- "$WECOM_PREFIX"
/usr/bin/wineboot --init
export WECOM_BACKUP="$WECOM_PREFIX/setup-backups"
mkdir -- "$WECOM_BACKUP"
wecom_winpath() { /usr/bin/winepath -w "$1"; }
```

`winemenubuilder.exe=d` 禁用 Wine 自动生成桌面菜单，使用第 10 节的专用启动入口以加载完整配置。

让新 prefix 的 Desktop、Documents 等目录各自独立，单独创建 Linux 启动入口：

```bash
python3 - <<'PY'
import getpass, os
from pathlib import Path
user_dir = Path(os.environ['WECOM_PREFIX']) / 'drive_c/users' / getpass.getuser()
for path in user_dir.iterdir():
    if path.is_symlink():
        path.unlink()  # 只移除新 prefix 内的映射，不删除宿主文件
        path.mkdir()
PY
```

此流程不复制旧注册表、AppData、Documents 或账号状态；首次登录由用户手动完成。

## 3. 安装指定企业微信版本

```bash
/usr/bin/wine "$WECOM_INSTALLER" /S
export WECOM_APP_DIR="$WECOM_PREFIX/drive_c/Program Files (x86)/WXWork"
export WECOM_VERSION_DIR="$WECOM_APP_DIR/5.0.12.6018"
test -f "$WECOM_APP_DIR/WXWork.exe"
test -d "$WECOM_VERSION_DIR/compatible_web"

# 安装器可能已启动企业微信；只结束这个新 prefix。
/usr/bin/wineserver -k
/usr/bin/wineserver -w
```

继续配置 Web 组件、Qt 嵌入和字体，再进行功能验收。

## 4. 配置 64 位 Web 组件与页对齐

```bash
test ! -e "$WECOM_VERSION_DIR/compatible_web.x86-original"
test ! -e "$WECOM_VERSION_DIR/compatible_web.x64-staged"
cp -a -- "$WECOM_CEF_SRC" "$WECOM_VERSION_DIR/compatible_web.x64-staged"
test ! -e "$WECOM_BACKUP/libcef.vendor.dll"
cp -p -- "$WECOM_VERSION_DIR/compatible_web.x64-staged/libcef.dll" "$WECOM_BACKUP/libcef.vendor.dll"
python3 "$WECOM_SETUP/scripts/align-cef-memory.py" \
  --source "$WECOM_BACKUP/libcef.vendor.dll" \
  --output "$WECOM_VERSION_DIR/compatible_web.x64-staged/libcef.page-aligned.dll"
mv -- "$WECOM_VERSION_DIR/compatible_web.x64-staged/libcef.page-aligned.dll" \
  "$WECOM_VERSION_DIR/compatible_web.x64-staged/libcef.dll"
python3 "$WECOM_SETUP/scripts/verify-materials.py" --only cef \
  --cef-profile wine-builtin --cef-layout page-aligned \
  --cef-dir "$WECOM_VERSION_DIR/compatible_web.x64-staged"
mv -- "$WECOM_VERSION_DIR/compatible_web" "$WECOM_VERSION_DIR/compatible_web.x86-original"
mv -- "$WECOM_VERSION_DIR/compatible_web.x64-staged" "$WECOM_VERSION_DIR/compatible_web"
```

替换的是整套 `WXWorkWeb.exe`、`libcef.dll`、配套 DLL、资源及 locales，目录名仍为 `compatible_web`。主程序保持安装器提供的 x86 原件。页对齐工具仅重排已知 `libcef.dll` 的文件布局，保留代码及 RVA；其签名会失效，必须保留 `libcef.vendor.dll` 原件并按独立指纹校验。

## 5. 配置运行库及 Qt DPI

为精简 CEF 安装运行库；保留主程序和 `wemail/` 中安装器自带的 DLL 及 manifest。配置完整列表在 [dll-overrides.txt](configs/dll-overrides.txt)：

```bash
env WINEPREFIX="$WECOM_PREFIX" WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q vcrun2022 ucrtbase2019 d3dcompiler_47
export WINEDLLOVERRIDES="$(cat "$WECOM_SETUP/configs/dll-overrides.txt")"
/usr/bin/wine reg.exe import "$(wecom_winpath "$WECOM_SETUP/configs/cef-builtin.reg")"
test -f "$WECOM_VERSION_DIR/msvcp140.dll"
test -f "$WECOM_VERSION_DIR/ucrtbase.dll"

test ! -e "$WECOM_VERSION_DIR/wemail/qt.conf"
cp -- "$WECOM_SETUP/configs/qt.conf" "$WECOM_VERSION_DIR/wemail/qt.conf"
```

11 个运行库由 winetricks 安装到系统目录；CEF 的 `vcruntime140_1.dll`、`dbghelp.dll` 使用 Wine 内建实现，46 个 API-set 名称由 Wine 解析。不要删除主程序和 WeMail 的本地运行库：它们的清单会固定加载路径，直接删除会导致启动或邮件、文档加载失败。无需编辑清单或 EXE。组件对应关系见 [winetricks 对照](materials/winetricks.md#微软-dll)，不要叠装多个 vcrun 年份。

`qt.conf` 内容：

```ini
[Platforms]
WindowsArguments = dpiawareness=1
```

DLL 的 `n,b` 表示优先微软原生实现、保留 Wine 内置回退。Qt 配置针对文档列表不能嵌入的问题，不需要改变 Linux 或 Wine 的整体 DPI 数值。

## 6. 安装字体及两种字体映射

先备份新环境原有键值，安装西文字体，再复制四个 CN OTF 并注册。以下 `.reg` 对应已验证的字体族与文件名；若改用其他中文字体或 TTC，须同步调整配置。

```bash
/usr/bin/wine reg.exe export 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\Fonts' "$(wecom_winpath "$WECOM_BACKUP/fonts.reg")" /y
/usr/bin/wine reg.exe export 'HKLM\Software\Microsoft\Windows\CurrentVersion\Fonts' "$(wecom_winpath "$WECOM_BACKUP/legacy-fonts.reg")" /y
/usr/bin/wine reg.exe export 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes' "$(wecom_winpath "$WECOM_BACKUP/substitutes.reg")" /y
/usr/bin/wine reg.exe export 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontLink\SystemLink' "$(wecom_winpath "$WECOM_BACKUP/links.reg")" /y

env WINEPREFIX="$WECOM_PREFIX" WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q corefonts calibri consolas tahoma micross

python3 - <<'PY'
import hashlib, json, os, shutil
from pathlib import Path
setup = Path(os.environ['WECOM_SETUP'])
entries = json.loads((setup / 'materials/fonts.json').read_text())
entries = [entry for entry in entries if entry['filename'].endswith('.otf')]
assert len(entries) == 4
dest = Path(os.environ['WECOM_PREFIX']) / 'drive_c/windows/Fonts'
flat = os.environ['WECOM_FONTS_SRC']
pairs = [(Path(flat) / x['filename'] if flat else setup / x['source'], dest / x['filename'], x) for x in entries]
for source, target, info in pairs:
    assert not target.exists(), f'目标字体已存在，先检查：{target}'
    assert hashlib.sha256(source.read_bytes()).hexdigest() == info['sha256'], source
for source, target, info in pairs:
    shutil.copy2(source, target)
    target.chmod(0o644)
    assert hashlib.sha256(target.read_bytes()).hexdigest() == info['sha256']
print('4 个 CN 字体已复制并核对；西文字体由 winetricks 安装')
PY

/usr/bin/wine reg.exe import "$(wecom_winpath "$WECOM_SETUP/configs/fonts-install.reg")"
/usr/bin/wine reg.exe import "$(wecom_winpath "$WECOM_SETUP/configs/fonts-replacements.reg")"
```

在此全新 prefix 中，`Fonts/Replacements` 原本不存在。对已有环境应用时要先查询并备份它，不能假定为空。

字体分工见 [配置说明](configuration-rationale.md#字体与输入)：字体文件、GDI `FontSubstitutes`、`FontLink` 和 Wine `Fonts/Replacements` 各有用途。Tahoma 等默认 FontLink 可能因区域设置变化被重写，不能仅凭导入成功就认定悬浮提示修复。

## 7. 配置 32 位微软 RichEdit

停止新 prefix 并备份原文件，通过 winetricks 安装匹配的 RichEdit，再核对架构和 SHA256：

```bash
/usr/bin/wineserver -k
/usr/bin/wineserver -w
test ! -e "$WECOM_BACKUP/riched20.dll"
cp -p -- "$WECOM_PREFIX/drive_c/windows/syswow64/riched20.dll" "$WECOM_BACKUP/riched20.dll"
if [[ -f "$WECOM_PREFIX/drive_c/windows/syswow64/msls31.dll" ]]; then
  cp -p -- "$WECOM_PREFIX/drive_c/windows/syswow64/msls31.dll" "$WECOM_BACKUP/msls31.dll"
else
  touch "$WECOM_BACKUP/msls31.was-absent"
fi
env WINEPREFIX="$WECOM_PREFIX" WINE=/usr/bin/wine WINESERVER=/usr/bin/wineserver \
  winetricks -q riched30
python3 "$WECOM_SETUP/scripts/verify-materials.py" --only richedit \
  --richedit "$WECOM_PREFIX/drive_c/windows/syswow64/riched20.dll"
```

已加载的覆盖列表含 `riched20=n,b`。注意 `syswow64` 放的是 x86 DLL，`system32` 放的是 x64 DLL。所核对 `riched30` 配方安装 x86 RichEdit 及配套 `msls31.dll`，不能按 DLL 文件名改用 `riched20` 包；来源比对见[说明](materials/winetricks.md#richedit-安装)。

## 8. 设置系统提示字体

```bash
/usr/bin/wine reg.exe export 'HKCU\Control Panel\Desktop\WindowMetrics' "$(wecom_winpath "$WECOM_BACKUP/window-metrics.reg")" /y
/usr/bin/wine "$WECOM_SETUP/scripts/set-status-font.exe"
```

这个小工具通过 `SPI_GET/SETNONCLIENTMETRICS` 将 `lfStatusFont` 字体名改为 `Source Han Sans CN`，沿用原字号和字重并保存设置。对应键为 `HKCU\Control Panel\Desktop\WindowMetrics\StatusFont`，不是一个简单的字符串注册表值；工具和源码一同留存，避免手写错误的 LOGFONT 二进制数据。

设置成功后输出 `StatusFont set to Source Han Sans CN.`。在下面完整停止并重启，让企业微信的提示控件重新创建。

## 9. 配置表格文件打开

Linux 端安装 WPS 表格，并确认应用列表中有 `wps-office-et.desktop`。以下为当前正式配置；如果使用其他表格程序，将两条 `xdg-mime` 命令中的桌面文件名换成对应应用。宿主默认应用由这些命令设置，Wine 端仅把 xls/xlsx 交给宿主打开。

```bash
xdg-mime default wps-office-et.desktop application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
xdg-mime default wps-office-et.desktop application/vnd.ms-excel
/usr/bin/wine reg.exe import "$(wecom_winpath "$WECOM_SETUP/configs/native-spreadsheet-open.reg")"
/usr/bin/wine reg.exe query 'HKCR\WeCom.NativeSpreadsheet\shell\open\command' /reg:32
```

本页目标是空 prefix；已有环境应先检查并备份相关关联，再按[表格打开说明](verification-and-recovery.md#聊天文件菜单与原生表格打开)处理。文件菜单的另存为和复制由下一节安装的窗口助手修复。

## 10. 启动与独立桌面入口

将下面配套文件复制到 prefix，日常启动便不依赖仓库目录。启动器检查失效共享锁，启动托盘图标和窗口辅助程序，再加载输入桥接并运行企业微信。各文件职责见[辅助文件清单](verification-and-recovery.md#辅助文件维护)。

```bash
test ! -e "$WECOM_PREFIX/wecom-compat"
mkdir -p -- "$WECOM_PREFIX/wecom-compat/scripts" "$WECOM_PREFIX/wecom-compat/configs"
cp -- "$WECOM_SETUP/scripts/launch-wecom.sh" "$WECOM_SETUP/scripts/prepare-startup.py" \
  "$WECOM_SETUP/scripts/tray-geometry.py" "$WECOM_SETUP/scripts/tray-icon.py" \
  "$WECOM_SETUP/scripts/titlebar-managed.exe" \
  "$WECOM_SETUP/scripts/ime-bridge.c" "$WECOM_SETUP/scripts/ime-bridge.so" \
  "$WECOM_SETUP/scripts/titlebar-managed.c" "$WECOM_PREFIX/wecom-compat/scripts/"
cp -- "$WECOM_SETUP/configs/dll-overrides.txt" "$WECOM_SETUP/configs/wine-vprot-runtime.sha256" \
  "$WECOM_SETUP/configs/native-spreadsheet-open.reg" \
  "$WECOM_PREFIX/wecom-compat/configs/"
/usr/bin/wineserver -k
/usr/bin/wineserver -w
bash "$WECOM_PREFIX/wecom-compat/scripts/launch-wecom.sh"
```

启动脚本使用 `WECOM_PREFIX`，显式固定原生 Wine、中文区域设置及完整 DLL 覆盖列表，并启动标题栏维护工具；工具随主进程退出。首次登录在企业微信窗口中自行完成。

当前使用系统 Wine 11.20，版本守卫停用仅适用于 11.19 的专用运行时。上面复制固定校验清单以保持启动器配置完整，不下载或构建 Wine。运行时选择见[维护说明](verification-and-recovery.md#wine-页保护元数据)。

需要桌面图标时，在另一个终端创建一个**新名字**的 `.desktop`，不要覆盖已有入口。将示例中的 `<user>` 替换为真实用户名；Desktop Entry 不展开 `$HOME`，必须使用实际绝对路径：

```ini
[Desktop Entry]
Name=企业微信（原生 Wine 重建）
Type=Application
Exec=env WECOM_PREFIX=/home/<user>/Works/Softwares/wine/WeCom-rebuild /bin/bash /home/<user>/Works/Softwares/wine/WeCom-rebuild/wecom-compat/scripts/launch-wecom.sh
Icon=applications-office
Terminal=false
StartupNotify=true
StartupWMClass=wxwork.exe
```

这个示例入口依赖 prefix 内 `wecom-compat/` 的文件；移动 prefix 后更新 `Exec`。原生 Wine 环境的 `WeCom-Native-Wine.desktop` 也使用这一布局。

最后执行[使用检查](verification-and-recovery.md#使用检查)，确认页面、中文输入与提示、标题栏、任务栏和各屏幕托盘菜单正常。材料校验通过不能代替实际功能检查。
