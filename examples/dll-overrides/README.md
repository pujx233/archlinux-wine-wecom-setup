# winecfg DLL 覆盖试验

在独立 Wine 11.19 `win64` prefix 中，对企业微信 5.0.11.6018 / CEF 5.0.79.5483 验证 winecfg 的 DLL 覆盖行为。结果见 [winecfg-validation.json](../../materials/winecfg-validation.json)，结论见[组件说明](../../materials/winetricks.md#winecfg-覆盖试验)。

这里保存自制加载探针源码，不提供微软 DLL、腾讯组件、账号状态或原始运行日志。独立试验结束后，30 文件配置已应用到正式环境；重启后用户确认页面、中文输入和显示正常。部署结果见 [winecfg-deployment.json](../../materials/winecfg-deployment.json)。

## 实际使用的覆盖配置

winecfg 在“应用程序”中选择对应 EXE，再在“函数库”中设为“内建”，对应下面这些注册表值。试验通过 `wine reg.exe import` 写入相同的键，没有依赖 GUI 自动化。

```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\Wine\AppDefaults\WXWorkWeb.exe\DllOverrides]
"*vcruntime140_1"="builtin"
"*dbghelp"="builtin"

[HKEY_CURRENT_USER\Software\Wine\AppDefaults\minidump.exe\DllOverrides]
"*vcruntime140_1"="builtin"
"*dbghelp"="builtin"

[HKEY_CURRENT_USER\Software\Wine\AppDefaults\TxBugReport.exe\DllOverrides]
"*vcruntime140_1"="builtin"
"*dbghelp"="builtin"
```

此配置对应已验证的 30 文件方案；新装在目标独立 prefix 应用，已有环境先保存工作、退出并备份。可直接导入仓库的 [cef-builtin.reg](../../configs/cef-builtin.reg)。配套启动器从 `WINEDLLOVERRIDES` 列表去掉了 `vcruntime140_1`，让注册表承担该项配置；其他 VC/UCRT、RichEdit 配置保持原方案。对不同程序的同名 DLL 不做全局切换。API-set 无需逐项导入覆盖，Wine 会将名称解析为实际系统模块。

三轮试验逐步从测试 CEF 目录移走 `vcruntime140_1.dll`、46 个 `api-ms-*.dll`、`dbghelp.dll`；应用主目录和 `wemail/` 的 DLL、manifest 均保留。对应的正式目录有 30 个文件，可用 `verify-materials.py --only cef --cef-profile wine-builtin` 校验。这里不是对任意版本 CEF 的通用删减指令；复现前须按原厂商清单确认版本。

## 编译与使用探针

`loader-probe.c` 只调用 Windows 加载 API，输出是否创建了激活上下文、加载错误码和模块名，不读取输入内容。它不依赖 VC/UCRT，以免提前加载被测运行库。需要 clang、lld 和本机 Wine 开发导入库。

下面的 `WECOM_PROBE_BUILD` 应是一个新的临时目录：

```bash
export WECOM_PROBE_BUILD="$(mktemp -d)"
for spec in '32 i686 i386' '64 x86_64 x86_64'; do
    read -r bits target wine_arch <<< "$spec"
    clang --target="$target-pc-windows-msvc" -ffreestanding -fno-stack-protector \
      -Wall -Wextra -Werror -c examples/dll-overrides/loader-probe.c \
      -o "$WECOM_PROBE_BUILD/probe$bits.obj"
    lld-link /entry:mainCRTStartup /subsystem:console /nodefaultlib /safeseh:no \
      "/out:$WECOM_PROBE_BUILD/probe$bits.exe" "$WECOM_PROBE_BUILD/probe$bits.obj" \
      "/usr/lib/wine/$wine_arch-windows/libkernel32.a"
done
```

在独立测试 prefix 中，通过以下环境变量选择测试内容：

| 变量 | 内容 |
|---|---|
| `WECOM_PROBE_DLL` | DLL 名或 Windows 完整路径，必填 |
| `WECOM_PROBE_ACTCTX` | 可选：外部 manifest 的 Windows 路径，或有内嵌 manifest 的 EXE 路径 |
| `WECOM_PROBE_RESOURCE=1` | 与 EXE 路径配合，选择 manifest 资源 1 |

32 位探针使用主程序的外部 manifest 或 WeMail 的内嵌 manifest，64 位探针用于 CEF。把探针复制到相应测试目录后，按 `probe32.exe` / `probe64.exe` 设置应用专用 `DllOverrides`，分别测试 `native,builtin`、`native`、`builtin,native`、`builtin`。各项测试都启动新进程，避免已加载模块的缓存干扰。

启用 `WINEDEBUG=-all,+loaddll`，并核对 Linux `/proc/<pid>/maps`。**不能只凭 `GetModuleFileName` 判断实现**：试验中 Wine 的内建 DLL 可能显示被重定向的应用目录路径，但加载日志标记为 `builtin`。32 位程序显示的 `system32` 还涉及 WOW64 目录重定向。

探针加载成功只证明加载层可用。页面、输入、长期稳定性、崩溃报告等必须分别验收，不能用单个 `LoadLibrary` 的成功代替。
