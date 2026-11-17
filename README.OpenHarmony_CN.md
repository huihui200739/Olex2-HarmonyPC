# Olex2 鸿蒙 PC 适配 — 构建与验证说明

本仓库把 Olex2 的原生 C++ 内核移植到 HarmonyOS PC（2in1）：上游源码按 commit 固定，改造以补丁形式维护，再通过 N-API 把原版主窗体、命令解释器与渲染链路接到 ArkTS 应用框架上。

- 上游：`pcxod/olex2` @ `ca2b4c1b68ac9d7d8fddc53eb5fcf60797d2d3e1`（见 [source.lock.json](source.lock.json)）
- 许可证：OlexSys BSD 三条款式（见 [LICENSE](LICENSE) 与 [upstream/LICENCE.txt](upstream/LICENCE.txt)）
- 目标设备类型：`2in1`（鸿蒙 PC）

## 环境

| 组件 | 版本/路径 |
|---|---|
| DevEco Studio | 自带 SDK，默认安装在 `/Applications/DevEco-Studio.app` |
| HarmonyOS SDK | OpenHarmony 6.1.1.125 / API 24（native 工具链） |
| Node.js | DevEco 自带（`tools/node`），或用系统 20.x |
| Python | 3.10+（构建脚本与验证脚本） |
| hdc | DevEco SDK `default/openharmony/toolchains/hdc` |

工程路径不要带中文：DevEco/Hvigor 会拒绝中文目录。

## 获取源码

```bash
git clone https://atomgit.com/OpenHarmonyPCDeveloper/ohos_Olex2.git
cd ohos_Olex2
```

> 上游 GitHub 镜像：`https://github.com/huihui200739/Olex2-HarmonyPC`（内容一致）

## 构建

```bash
# 1) 本机原生层构建 + 冒烟测试（host，跑 ctest）
scripts/build-baseline.sh host

# 2) 交叉编译到 OHOS arm64
scripts/build-baseline.sh ohos

# 3) 构建未签名 debug HAP
#    产物：out/gx-application-evaluation/app-builds/<构建ID>/entry-default-unsigned.hap
scripts/build-app.sh
```

`build-app.sh` 会把工作树复制到临时 stage、写入构建来源清单 `OLEX2-BUILD-INPUTS.json`，调用 `hvigorw assembleHap`，并产出 `provenance.json`（记录工具链哈希与 HAP 字节绑定）。

## 一条命令验证

```bash
scripts/verify.sh                    # 来源核对 + 原生构建/冒烟 + HAP 构建
VERIFY_SKIP_APP=1 scripts/verify.sh  # 只跑前两步，不建 HAP
```

设备回归（需要模拟器或真机在线）：

```bash
python3 scripts/run-app-smoke.py --target 127.0.0.1:5555 --hap out/app/entry-default-unsigned.hap
python3 scripts/check-adaptation-status.py   # 只核对已保留的收据，不启动应用
```

## 安装

```bash
hdc list targets
hdc install out/gx-application-evaluation/app-builds/<构建ID>/entry-default-unsigned.hap
```

仓库提供的是**未签名 debug HAP**，设备需要允许安装调试包；正式分发需自行完成签名。

## 目录

| 路径 | 内容 |
|---|---|
| `app/` | HarmonyOS 工程（ArkTS 页面 + C++ 原生库） |
| `native/` | 适配层：ArkUI 宿主、N-API 桥、原版窗口/命令/对话框接入 |
| `patches/` | 对上游的 122 份补丁，逐字节可重放，SHA-256 记录在 `source.lock.json` |
| `cmake/` | 上游库构建模块与导出符号白名单 |
| `upstream/` | 固定的上游 Olex2 源码（勿直接改） |
| `vendor/` | cpython / libffi / zlib / libpng / gl4es / glu |
| `scripts/` | 构建、来源核对、设备验证脚本（见 `scripts/README.md`） |
| `tests/fixtures/` | 设备验证脚本的输入样例 |
| `docs/` | 设计与验收文档（索引见 `docs/README.md`） |
| `reports/` | 小体量验证收据 |

## 已验证 / 未验证

已验证（模拟器，OpenHarmony-6.1.1.125 / API 24 / 2in1）：主窗体四项菜单与状态栏、命令栏、文件格式载入与保存、绘图样式与主要对话框、选择/片段/编组、原子与键工具、晶胞与晶向、Python 脚本入口、多文档与窗口缩放。逐项状态见 `docs/原版功能适配清单-2026-11-14.md`。

未验证或不在范围：物理鸿蒙 PC 真机、DPI 缩放与多显示器、真实键盘与输入法、剪贴板、插件体系、html 面板、文件关联。界面按原版主框架对齐，不是 wx 界面的逐像素复刻。

## 问题排查

- `Olex2 source missing`：`upstream/` 不全，重新 clone 或核对 `source.lock.json`
- `hvigorw: command not found`：DevEco 不在默认路径，调整脚本里的 `DEVECO` 或把 SDK 加进 PATH
- 构建报 `native/... not found`：`patches/` 未按顺序应用，或 `upstream/` 版本不对
