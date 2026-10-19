# Wine 页保护元数据内存修复

在 Wine **11.19-1 / Linux x86_64 / 4 KiB 宿主页 / 新 WoW64** 下完成底层 A/B、隔离企业微信和正式环境功能验收。它与 [CEF 文件页对齐](../cef-memory/README.md)是两个独立修复；本次没有关闭文档预加载，也不包含另一个目录的实验 IME 补丁。

## 根因与结果

Wine 的 `dlls/ntdll/unix/virtual.c` 为每个 4 KiB Windows 页存一个保护字节。CEF/V8 仅预留 1 TiB 的 `PAGE_NOACCESS` 地址时，`set_page_vprot()` 的 `memset(0)` 就会写入 **256 MiB** 元数据。这里是 Wine **用户态匿名内存**，不是实际分配了 1 TiB，也不是每个进程独占的 Linux 内核 slab。

[wine-11.19.patch](wine-11.19.patch) 只改变 64 位页保护字节的清零路径：独立匿名映射中完整的宿主页用 `MADV_DONTNEED` 恢复按需零页，首尾不完整页继续 `memset`；非零保护值、非 Linux 路径和失败回退保持 `memset`。不改变二级表指针、预留地址大小或 V8 沙箱。

同源码、同配置的三轮 1 TiB 预留探针：基线 PSS 每轮增加 **262148 KiB**，候选增加 **11 / 8 / 8 KiB**；两组无访问映射均增加恰好 1 TiB，Swap 为 0，实际 `ntdll.so` 映射逐次核对。每轮 467740 次自定义内存语义断言通过；原样 Wine `ntdll` 虚拟内存测试两组各 2550 项、112 个既有 TODO、0 失败。精确抽出的辅助函数在 221184 个案例及成功／失败 `madvise` 分支通过历史 sanitizer 验证。

隔离应用中 31 个 Wine 进程、7 个 renderer，在 246.7 秒内总 PSS 从 2875.46 增至 2894.22 MiB，PID 集合不变、窗口响应正常。**有轻微增长，不能宣称无长期泄漏**。用户确认文档正文、邮件页和中文输入正常后，已切换正式环境：7 个 renderer 的正式样本总 PSS **3007.55 MiB（约 2.94 GiB）**，再次获得用户功能确认。它和旧启动的页面历史不同，不把应用总量差额当作严格 A/B 收益。详见 [results.json](results.json)及[正式记录](../../materials/wine-vprot-deployment.json)。

## 安装已验证产物

仓库保存补丁、探针、校验清单及安装工具，**不分发 Wine 二进制或账号数据**。需要本机已保存、与[固定清单](../../configs/wine-vprot-runtime.sha256)一致的运行时目录；不是从任意 Wine 安装中提取 `ntdll.so`。目录须含 `lib/wine/x86_64-unix/{ntdll.so,wine,wine-preloader}` 三个普通文件。

在仓库根目录执行，输出必须是新目录：

```bash
set -euo pipefail
runtime="${XDG_DATA_HOME:-$HOME/.local/share}/wecom-runtimes/wine-11.19-vprot-e38a364b"
python3 scripts/install-wine-vprot-runtime.py \
  --source /path/to/validated-runtime --output "$runtime"
```

当前正式机器该目录已存在，不必重装。安装器核对系统包版本、三个候选文件和三个系统依赖的哈希；复制 loader、preloader、候选 ntdll，只读链接本机 PE/其他 Unix 模块和 Wine 资源。输出已存在、哈希不符或 preloader 为符号链接时拒绝安装。失败仅清理此次新建输出；不会修改 prefix、系统 Wine 或运行中的企业微信。

**preloader 必须复制，不能链接**，否则 `/proc/self/exe` 可能将加载位置导回系统 Wine，导致补丁未生效。`win32u.so` 保留系统链接，原有 IME 桥接仍使用同一模块，不叠加实验输入修复。

随后按[更新启动辅助文件](../../verification-and-recovery.md#更新启动辅助文件)保存工作、停止目标 prefix、备份并同步启动器与 `configs/wine-vprot-runtime.sha256`，再启动。默认查找上面的用户数据目录；没有安装时安静使用 `/usr/bin/wine`。自定义位置通过**绝对路径** `WECOM_VPROT_RUNTIME` 指定；只修改该变量不会安装任何文件。

新启动器校验的是随启动器安装的清单，不信任运行时自带清单来放宽检查。系统 Wine 包升级、文件哈希不符、配置缺失或加载器错误时警告并退回系统 Wine。主程序和标题栏辅助程序统一选择同一个 Wine；启动时清除继承的 `WINELOADERNOEXEC`，保留原有中文区域设置、DLL 覆盖及 IME 桥接。Wine 运行期间不要升级其共享依赖，升级后须完整退出并重新启动。

## 验收与回退

完整退出后再改变运行时或开关，不能把新启动方式接到旧 wineserver 会话并宣称切换成功。核对 `WINEPREFIX`、每个 Wine 应用进程的 `/proc/PID/maps` 中实际 `ntdll.so` 路径及哈希，确认没有遗留隔离实例。总物理内存比较使用进程 PSS，不累加 RSS；还需记录 renderer 数和页面负载。

人工验收聊天切换、文档正文、邮件页及中文试打后清空，不发送或保存测试内容。若出现挂起、崩溃或功能回归，先保存工作、完整停止目标 prefix；设置 `WECOM_VPROT_FIX=0` 后按原入口启动即可回退运行时，不恢复旧账号数据：

```bash
env WECOM_VPROT_FIX=0 WECOM_PREFIX="/path/to/WeCom" \
  bash "/path/to/WeCom/wecom-compat/scripts/launch-wecom.sh"
```

此变量只影响该次启动，之后普通桌面入口仍默认启用；需要持续停用时在自己的桌面入口 `Exec=env ...` 中保留 `WECOM_VPROT_FIX=0`，或停止后恢复更新前的启动器备份。不要删除正在被映射的专用运行时。

当前机器首次正式切换的备份另保存在 `$HOME/.local/state/wecom-vprot-production-20261016/`，包含 `control.py rollback`。它仅匹配那次启动器哈希；后续更新了启动器时应使用最新辅助文件备份，不能强行运行旧回退。全套从零安装、其他 Wine 版本和长期稳定性尚未验收。

## 源码复建与再验证

源码为 [Wine 11.19 标签归档](https://github.com/wine-mirror/wine/archive/refs/tags/wine-11.19.tar.gz)。先自行取得归档；本项目不在启动时下载、编译或应用补丁。需要 Wine Unix 构建依赖、GCC、make、patch；PE 探针另需 Wine 开发工具、Clang 和 LLD。

以下在仓库根目录运行，仅在新的临时目录复建基线和候选，不执行 `make install`：

```bash
set -euo pipefail
repo="$PWD"
archive="/path/to/wine-11.19.tar.gz"
printf '%s  %s\n' 73eb0b02dfc0cb0ff4f4d8d36c5786261e9725ac91968ae4d4e4b9a498c4087f "$archive" | sha256sum -c -
work="$(mktemp -d /tmp/wecom-vprot-build-XXXXXXXX)"
tar -xf "$archive" -C "$work"
mkdir "$work/build"
cd "$work/build"
CFLAGS='-g -O2' "$work/wine-wine-11.19/configure" --enable-win64 --without-x --without-wayland
make -j2 dlls/ntdll/ntdll.so loader/wine
cp dlls/ntdll/ntdll.so "$work/ntdll.baseline.so"
patch --batch -d "$work/wine-wine-11.19" -p1 < "$repo/experiments/wine-vprot/wine-11.19.patch"
make -j2 dlls/ntdll/ntdll.so loader/wine
cp dlls/ntdll/ntdll.so "$work/ntdll.candidate.so"
sha256sum "$work/ntdll.baseline.so" "$work/ntdll.candidate.so" loader/wine
```

**构建成功不等于已验证产物**。调试路径或工具链不同可能改变二进制哈希，安装器会拒绝未记录的产物；不得仅为绕过检查重写清单。先为新产物重复同条件底层 A/B、Wine 上游内存测试、32 位主程序和隔离应用验收，再评审更新清单。只有原地相同构建的重复 `make` 已证明哈希一致，不宣称跨机器可复现相同二进制。

探针复建（输出到上面的临时目录）：

```bash
cd "$repo"
clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -c experiments/wine-vprot/reserve-probe.c -o "$work/reserve-probe.obj"
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib \
  "/out:$work/reserve-probe.exe" "$work/reserve-probe.obj" \
  /usr/lib/wine/x86_64-windows/libkernel32.a
winegcc -b x86_64-windows --cc-cmd=clang -O2 \
  experiments/wine-vprot/semantics-probe.c -o "$work/semantics-probe.exe"
python3 -m unittest discover -s tests -p 'test_vprot_runtime.py' -v
```

`reserve-probe` 在 `BASELINE`、`RESERVED_1_TIB`、`FREED` 各等待 8 秒用于采样。为基线和候选分别准备专用运行时及空闲测试 prefix，设置对应 `WINEPREFIX`、`WINEDLLPATH=/usr/lib/wine`、`WINESERVER=/usr/bin/wineserver`；不要在正式 prefix 上运行探针。每次采样要确认实际 ntdll 映射，而非仅检查启动命令。`semantics-probe` 覆盖跨表、提交／保护／撤销提交／释放、邻区和并发，GUARD 访问异常由上游测试补充。

仓库离线回归从补丁原样抽取函数编译 `range-unit.c`，覆盖 4096 种偏移、首尾、非零值与 `madvise` 注入失败；不会启动 Wine、停止进程或复跑历史 GUI 验收。REA 反编译只作静态交叉证据，不代替上述行为测试。原始诊断日志、二进制、用户目录和业务页面信息不进入仓库。
