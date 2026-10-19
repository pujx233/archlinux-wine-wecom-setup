# CEF 映像共享内存修复

在 Wine 11.19、企业微信 5.0.11.6018、x64 CEF 5.0.79.5483 上验证。工具仅支持[已记录 SHA256](../../materials/cef64-page-aligned.json) 的一个 DLL，不是通用 PE 修改器。第三方 DLL、账号、截图和原始业务日志不进入仓库。

## 原因与结果

厂商 DLL 的 `.text` 文件偏移是 `0x600`，`.rdata` 是 `0x0c5e7e00`，均未对齐 Linux 的 4096 字节页。Wine 的 [`map_file_into_view`](https://github.com/wine-mirror/wine/blob/wine-11.19/dlls/ntdll/unix/virtual.c#L2342) 遇到 `mmap` 的 `EINVAL` 会退回 `pread`，将内容读入每个进程自己的匿名映像。这里的大块内存是用户态 CEF 映像，不是每个进程独占数百 MiB 的内核 slab。

[align-cef-memory.py](../../scripts/align-cef-memory.py) 将原始段文件偏移和文件对齐改为 4096，保持 RVA 与代码段字节不变，更新调试目录的文件指针，清空 PE checksum 和证书目录。输出增加 51,712 字节；原有签名及厂商完整性校验失效。输入、输出均锁定 SHA256，拒绝原地修改、未知版本和覆盖已有输出；不会改启动器或自动安装 DLL。

两进程探针逐页读入可读且非 discardable 的段后，合计 PSS **510.27 → 295.75 MiB**，不是单纯延迟加载。旧采样的 CEF 子范围漏掉尾部约 1.4 MiB，但整进程 PSS 完整。该范围内每进程私有脏页约 **231.24 → 14.81 MiB**，并出现约 216.45 MiB 的共享干净页。

真实应用隔离桌面测试通过启动和登录；用户确认文档正文、邮件页面正常。登录后的 7 个 CEF 进程、2 个 renderer 时，总 PSS 约 **2.42 GiB**；打开页面后达到 11 个 CEF 进程、6 个 renderer，总 PSS 约 **4.66 GiB**。此时每进程 CEF 映像私有脏页仅 **14.84–15.27 MiB**，11 个映像合计 PSS 约 **308.34 MiB**。页面自身的 JS、DOM、图片等内存仍然存在；不能把不同页面数量的总占用相减当作严格 A/B 节省量。数字和边界见 [results.json](results.json)。

早期 Xvfb 原版试验因会话过期中断，不算修复版页面验收；最终验收在真实桌面的独立 prefix 完成。复制登录状态可能让原实例掉线，不能把桌面原实例的登录误当成测试实例登录。验收需同时核对目标 `WINEPREFIX`、进程 PID、实际 DLL 映射路径和 SHA256。

隔离验收后已按用户要求[部署到正式环境](../../materials/cef-memory-deployment.json)，只替换 DLL，保留原启动辅助文件及账号数据。用户确认正式页面与常用中文输入正常；11 个 CEF、6 个 renderer 时总 PSS 4.72 GiB，CEF 映像合计 PSS 311.52 MiB。原件备份位于正式 prefix 的 `setup-backups/cef-memory-20261016-fg5wyhpb/libcef.dll`，签名风险及长期验证边界仍然适用。

## 生成、安装与回退

以下从仓库根目录运行。先使用隔离 prefix 验证，不要直接复制未经验证的其他版本 DLL。输出目录必须是新目录：

```bash
set -euo pipefail
export WECOM_PREFIX="/path/to/isolated-prefix"
cef_dir="$WECOM_PREFIX/drive_c/Program Files (x86)/WXWork/5.0.11.6018/compatible_web"
work_dir="$(mktemp -d)"
python3 scripts/align-cef-memory.py --source "$cef_dir/libcef.dll" --output "$work_dir/libcef.dll"
```

保存工作并完整停止**目标 prefix**，备份后安装。不要覆盖仍被进程映射的文件，也不要关闭其他 prefix：

```bash
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -k
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -w
mkdir -p "$WECOM_PREFIX/setup-backups"
backup_dir="$(mktemp -d "$WECOM_PREFIX/setup-backups/cef-memory-XXXXXXXX")"
cp -- "$cef_dir/libcef.dll" "$backup_dir/libcef.dll"
cp -- "$work_dir/libcef.dll" "$cef_dir/libcef.dll"
python3 scripts/verify-materials.py --only cef --cef-profile wine-builtin \
  --cef-layout page-aligned --cef-dir "$cef_dir"
```

按原启动方式重启并验收。`--cef-layout` 与文件数量方案独立；默认 `vendor` 仍严格检查厂商哈希，其余 29 个文件不放宽校验。`package-cef-example.py` 仍仅接受厂商原件，不打包修改后的 DLL。应用升级后不能沿用本修复，需要重新调查、生成指纹和验收。

回退：先保存工作并停止同一 prefix，再恢复本次备份，使用默认厂商布局校验后重启：

```bash
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -k
env WINEPREFIX="$WECOM_PREFIX" /usr/bin/wineserver -w
cp -- "$backup_dir/libcef.dll" "$cef_dir/libcef.dll"
python3 scripts/verify-materials.py --only cef --cef-profile wine-builtin --cef-dir "$cef_dir"
```

## 探针复建与核查

[probe.c](probe.c) 只加载指定 DLL、读取版本、按需触页并等待 25 秒供 `/proc/PID/smaps` 采样，不初始化完整业务页面。需要 clang、lld 和 Wine 的 x64 kernel32 导入库：

```bash
clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector \
  -c experiments/cef-memory/probe.c -o /tmp/wecom-memory-probe.obj
lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /timestamp:1792118150 \
  /out:/tmp/wecom-memory-probe.exe /tmp/wecom-memory-probe.obj \
  /usr/lib/wine/x86_64-windows/libkernel32.a
sha256sum /tmp/wecom-memory-probe.exe
python3 -m unittest discover -s tests -p 'test_cef_memory.py' -v
```

本机复建 SHA256 与实测探针相同，记录在 `results.json`。REA/Ghidra 对该探针入口反编译，确认 DLL 加载、版本函数调用、可读非 discardable 段的 4096 步长循环及等待；这不等于反编译验证了整个 CEF 或 Wine 加载器。对齐工具另用实际原件复现输出 SHA256，并检查段内容及调试文件指针。

运行探针需要独立、已配置相同运行库的 prefix：`WECOM_MEMORY_DLL` 为 Windows 格式 DLL 路径，`WECOM_MEMORY_DIRECTORY` 为依赖目录，`WECOM_MEMORY_MODE` 可为 `map`、`load`、`touch`；每组同时运行两份，读到两份 `READY` 后采样。不要把 RSS 相加当作物理占用，应比较合计 PSS，并检查 `Private_Dirty` / `Shared_Clean`。

## 文档预加载

日志已确认文档首页、占位文档和 PreloadService 预加载；`wedoc.config.ini` 存在 `pre_create_list_*` / `used_pre_create_list_*`。WeMail 二进制含 `wedoc/prepare_enabled`、`wedoc/prepare_advanced` 字符串，但未证实配置读取位置、默认值和关闭效果。本次**不写入猜测开关，不关闭整个 WeMail**；修复映像共享与关闭预加载是两个独立问题。
