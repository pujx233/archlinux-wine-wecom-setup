# Wine 11.19 整段输入投递实验

这是应用专用 Wine 修复的隔离原型，不是日常启动器使用的输入模块。生产 `scripts/ime-bridge.c/.so` 保持原实现；本目录补丁不由安装流程自动部署。

旧桥接逐个跨进程投递 UTF-16 `WM_CHAR`，中途失败时仍抑制整段原始 XIM 提交，可能丢失后半段。本实验通过一条 Wine 私有 `MSG_NOTIFY` 携带完整 UTF-16 缓冲区。服务器复制整段数据并入队后返回接受；接收线程向固定 HWND 同步分发 `WM_CHAR`。发送端只在接受后抑制原始提交，不超时重试，不转向新焦点。单次限制为 1–65,536 个 UTF-16 单元，超限或拒绝时保留原 XIM 返回值；原 Wine 路径是否能处理该提交仍不保证。

它消除了**逐字跨进程入队**的部分成功窗口，**没有提供编辑器事务或持久化保证**。接收窗口可以在首字符处理时销毁；进程也可能在队列接受后退出。这时发送端无法由“已接受”推断内容已完整写入。`WM_CHAR` 同步分发也改变了相对普通 posted 消息的调度次序，实际 CEF 编辑器兼容性仍需验证。

没有直接转发 Wine 的 `WINE_IME_POST_UPDATE`：其更新 ID 对应发送进程的本地缓冲区，跨进程只转发该 ID 不成立；接收端 HIMC、HKL 和组合状态也不能据此假定正确。本实验未接入标准 IMM 预编辑、候选窗或组合输入协议。

## 整段提交验证

| 检查 | 结果 |
|---|---|
| 仓库回归 | 22 项通过；其中启动 C harness 内含 7 个场景 |
| 64 位发送 → 64 位接收 | 69,895 个 UTF-16 单元，逐单元内容和顺序一致，无重复 |
| 32 位发送 → 64 位接收 | 同上；接收进程映射和发送端能力标识确认实验库生效 |
| 无效长度、空指针、无效窗口 | 拒绝请求，接收单元数保持 0 |
| 接收窗口处理中销毁 | 接受 3 个单元，收到第 1 个后退出；确认不具备编辑器事务保证 |
| 真实企业微信及 fcitx 输入链 | 未验收；未替换日常环境 |

## 文件与验证

- `wine-11.19.patch`：增加能力探测和整段消息，检查长度后打包／解包，在接收线程固定目标分发。
- `bridge.patch`：应用到生产桥接源码的实验差异。保留进程、CEF 类、XIM 宿主与焦点检查；只加载 `WECOM_IME_WIN32U` 指定的已映射库，并要求能力标识 `0x57494331`。
- `probe.c`：两个真实 Windows 进程的 UTF-16 内容、数量、顺序、重复和销毁边界探针，支持 32／64 位。
- `bridge-probe.c`、`../../tests/test_experimental_ime.py`：对临时应用补丁后的源码进行拒绝／接受和作用域回归，不启动 Wine。
- `run-isolated.sh`：固定使用 `/tmp/wecom-complete-fix/prefix`，结束后只关闭这个 prefix 的 wineserver。需在虚拟 X 显示器中运行。
- `results.json`：本次实际验证结果与限制；原始日志另保留在仓库 `dist/wine-ime-commit-20261013/`（Git 忽略）。

探针的成功只证明传输和测试窗口行为，不代表 fcitx → XIM → 桥接 → 企业微信 CEF 的完整输入链已验收。32 位 PE 探针还需传入现有 WoW64 thunk 要求的四字段参数缓冲区；实验桥接调用的是 x86_64 Unix ABI，不经该 PE thunk。

## 重现构建

验证环境为 Arch x86_64、系统 Wine 11.19-1 新 WoW64。需要 C 编译器、Clang、LLD、Wine 开发导入库、Wine Unix 构建依赖、patch、Xvfb。源码来自 [Wine 11.19 标签](https://github.com/wine-mirror/wine/tree/wine-11.19)，压缩包 SHA-256 为 `73eb0b02dfc0cb0ff4f4d8d36c5786261e9725ac91968ae4d4e4b9a498c4087f`。以下为新实验目录的命令；已有已打补丁源码不要重复应用。

```bash
set -euo pipefail
repo="$PWD" # 仓库根目录
work=/tmp/wecom-complete-fix
mkdir -p "$work/wine-build"
curl --fail --location --silent --show-error \
  https://codeload.github.com/wine-mirror/wine/tar.gz/refs/tags/wine-11.19 \
  -o "$work/wine-11.19.tar.gz"
printf '%s  %s\n' \
  73eb0b02dfc0cb0ff4f4d8d36c5786261e9725ac91968ae4d4e4b9a498c4087f \
  "$work/wine-11.19.tar.gz" | sha256sum -c -
tar -xzf "$work/wine-11.19.tar.gz" -C "$work"
patch -p1 -d "$work/wine-wine-11.19" < experiments/wine-ime-commit/wine-11.19.patch
cd "$work/wine-build"
../wine-wine-11.19/configure --enable-win64 --without-mingw --disable-tests
make -j8 dlls/win32u/win32u.so
cd "$repo"
```

只复制系统 loader／ntdll 到隔离目录，其他系统库及语言资源通过链接只读复用，替换该目录中的 `win32u.so`。不能只设置 `WINEDLLPATH`：默认安装目录优先，且部分 Unix 库按 PE 来源构造绝对路径。需要完整的隔离目录布局：

```bash
python3 - <<'PY'
from pathlib import Path
import shutil
work = Path('/tmp/wecom-complete-fix')
root = work / 'isolated'
unix = root / 'lib/wine/x86_64-unix'
unix.mkdir(parents=True, exist_ok=True)
for name in ('wine', 'wine-preloader', 'ntdll.so'):
    shutil.copy2(Path('/usr/lib/wine/x86_64-unix') / name, unix / name)
shutil.copy2(work / 'wine-build/dlls/win32u/win32u.so', unix / 'win32u.so')
for source in Path('/usr/lib/wine/x86_64-unix').iterdir():
    target = unix / source.name
    if not target.exists() and not target.is_symlink():
        target.symlink_to(source)
for arch in ('x86_64-windows', 'i386-windows'):
    target = root / 'lib/wine' / arch
    if not target.exists():
        target.symlink_to(Path('/usr/lib/wine') / arch)
(root / 'share').mkdir(exist_ok=True)
if not (root / 'share/wine').exists():
    (root / 'share/wine').symlink_to('/usr/share/wine')
PY
```

编译两种 PE 探针及实验桥接（仅生成文件，不加载桥接）：

```bash
clang --target=x86_64-w64-windows-gnu -O2 -ffreestanding -fno-builtin \
  -fno-stack-protector -Wall -Wextra -Werror -c experiments/wine-ime-commit/probe.c \
  -o /tmp/wine-ime-commit-probe.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib \
  /out:/tmp/wine-ime-commit-probe.exe /tmp/wine-ime-commit-probe.obj \
  /usr/lib/wine/x86_64-windows/libkernel32.a /usr/lib/wine/x86_64-windows/libuser32.a
clang --target=i686-w64-windows-gnu -O2 -ffreestanding -fno-builtin \
  -fno-stack-protector -Wall -Wextra -Werror -c experiments/wine-ime-commit/probe.c \
  -o /tmp/wine-ime-commit-probe32.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
  /out:/tmp/wine-ime-commit-probe32.exe /tmp/wine-ime-commit-probe32.obj \
  /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a
patch -o /tmp/wecom-complete-fix/ime-bridge.c scripts/ime-bridge.c \
  < experiments/wine-ime-commit/bridge.patch
cc -shared -fPIC -O2 -Wall -Wextra /tmp/wecom-complete-fix/ime-bridge.c \
  -o /tmp/wecom-complete-fix/ime-bridge.so -ldl
python3 -m unittest discover -s tests -v
env -u LD_PRELOAD xvfb-run -a bash experiments/wine-ime-commit/run-isolated.sh
```

运行脚本会先用系统 Wine 引导缺少基础 DLL 的测试 prefix，再改用实验 loader。它检查接收进程 `/proc/<pid>/maps` 确认实验 `win32u.so` 已加载；发送端还检查能力标识，避免把系统 Wine 的结果记为补丁成功。第一次 Wine 启动可能较慢。测试结束不自动删除源码、prefix 或日志，便于复核。

## 后续应用验收

必须使用另一个明确隔离、具备企业微信及 CEF 的应用测试环境，验证邮件正文、文档、审批、焦点切换、撤销、长文本和组合输入。当前测试 prefix 只含 Wine 与合成探针，没有企业微信账号环境。未执行真实消息发送、邮件收发或审批提交。真实应用验收前，不应把本目录补丁并入日常启动器或宣称完整修复。
