# Olex2 鸿蒙 PC 适配验证记录（2026-11-17）

## 代码基线

| 项 | 值 |
|---|---|
| 仓库 | `huihui200739/Olex2-HarmonyPC`（GitHub），计划镜像为 AtomGit `OpenHarmonyPCDeveloper/ohos_Olex2` |
| 版本 | 0.22.62 |
| 构建时刻的 main | `b507f0e`（此后 `22079a3` 只改 `scripts/build-app.sh` 的 mkdir 与 README，不动应用代码） |
| 上游 | `pcxod/olex2` @ `ca2b4c1b68ac9d7d8fddc53eb5fcf60797d2d3e1`，122 份补丁（`source.lock.json` 记录每份 SHA-256） |

## 本次构建

| 项 | 值 |
|---|---|
| 构建时间 | 2026-11-17 13:42（UTC+8） |
| 构建 ID | `20261117-134249-90d99e` |
| 入口 | `scripts/build-app.sh`（`hvigorw assembleHap`，debug） |
| 产物 | `entry-default-unsigned.hap` |
| 大小 | 36,460,545 字节 |
| HAP SHA-256 | `b3788953873ff5a3c2504d72cc6a56b4885dd0a51e6d8e65cfa766ef1e8963fa` |
| 来源记录 | `entry-default-unsigned.provenance.json`，SHA-256 `e06e7ed3fdceb7073ad78a95a02dc7266d520b234701bc972211bf0d7d73d3de` |
| 工具链 | OpenHarmony 6.1.1.125 / API 24 native；DevEco Studio 自带 cmake / ninja / clang |

## 本轮实际执行

1. 源码完整性：按 tarball 取回工作树，目录结构与 `CMakeLists.txt` 引用的 40 个 `native/` 源文件、`cmake/` 模块全部对得上
2. 原生库构建：`BuildNativeWithCmake` 通过
3. ArkTS 整页编译：0 error，告警为既有的 `getContext` / `show` 弃用提示
4. HAP 打包 + provenance 校验：`build_provenance_ffi_v1.py verify` 通过

## 验证范围与来源

- 上游 442 项 C++ 源码：442/442 编译与严格链接。该资格取自 0.22.62 既有全量记录（`experiments/original-document62/cpp442-main-gui-v1/native-build-v4/verification.json`，已随历史归档），**本轮未重跑全量**
- 功能清单 52 项：48 已验证 / 4 部分，逐项状态与证据见 [原版功能适配清单-2026-11-14.md](原版功能适配清单-2026-11-14.md)
- 本轮**未执行设备回归**（构建环境无在线模拟器），不新增设备断言

## 界面截图

截图来自 2026-11-16 的构建，早于本次 11-17 构建，仅用于展示界面形态，不作为 0.22.62 的运行证据。

| 文件 | 内容 | SHA-256 |
|---|---|---|
| `docs/screenshots/ui-original-menubar.jpeg` | 原版四项菜单 + 状态栏 | `ebd69bfad28e6e7030b3ca7d5fb4398b43637deacb2e17c3930c5f6315d4981c` |
| `docs/screenshots/ui-original-frame-document.jpeg` | 文档区 + MDI 子窗口标题 | `77df9b0ec98314bdb72126b3347d54acba07c5b6055da9d2e195ba249997cc95` |
| `docs/screenshots/window-title-olex2.jpeg` | 窗口标题与图标 | `9256ab1cd392bb8fab65bb50c8bfda57c3bb17c07bfc0fd043c7bea67bf79a6a` |

提交侧载站前建议用 0.22.62 的 HAP 在 2in1 设备/模拟器上重拍完整桌面截图。

## 已知边界

- 未签名 debug 包，不能直接用于分发；需要签名后才能在普通设备安装
- 物理鸿蒙 PC 未验证；DPI 缩放、多显示器、真实键盘与输入法、剪贴板未覆盖
- 插件体系、html 面板、文件关联不在适配范围
- 界面按原版主框架对齐，不是 wx 界面的逐像素复刻
