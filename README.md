# Olex2 · 鸿蒙 PC 适配

把开源晶体学软件 [Olex2](https://github.com/pcxod/olex2) 的原生 C++ 内核移植到 HarmonyOS PC：
上游源码按 commit 固定、以补丁方式改造，再把 `TGXApp`/渲染链路经 N-API 接到 ArkTS 应用框架上。

| 项 | 值 |
|---|---|
| 上游 | `pcxod/olex2` @ `ca2b4c1b68ac9d7d8fddc53eb5fcf60797d2d3e1`（[source.lock.json](source.lock.json)），OlexSys BSD 三条款式许可 |
| 应用工程 | `app/`（HarmonyOS，DevEco/ArkTS + C++ N-API） |
| 适配层 | `native/`（203 个文件：ArkUI 宿主、N-API 桥、原版窗口/菜单/对话框接入） |
| 上游改造 | `patches/`（122 份补丁，逐字节可重放） |
| 目标平台 | OpenHarmony 6.1.1.125 / API 24，arm64；当前只用模拟器验收 |

## 当前状态（0.22.62 开发树）

| 指标 | 状态 |
|---|---|
| 上游源码编译资格 | **442 / 442**（438 份进应用原生库，4 份原 `main` 单独严格链接） |
| 范围内功能清单 | 52 项：**48 已验证 / 4 部分 / 0 未验证**（严格口径 ≈96.2%） |
| 验证方式 | 未签名 debug HAP + 模拟器（`127.0.0.1:5555`）逐项交互与状态读回 |
| 未覆盖 | 物理 PC、正式签名、DPI 缩放/多显示器、真实键盘与输入法、插件与 html 面板 |

功能清单逐项状态见 [原版功能适配清单](docs/原版功能适配清单-2026-11-14.md)，批次历史见 [CHANGELOG](CHANGELOG.md)。
"442/442" 只说明源码能编译、能严格链接，**不等于**整体功能完成率。

## 下载

未签名 debug 构建（0.22.62）：[Releases → 0.22.62](https://github.com/huihui200739/OleX2-HarmonyPC/releases/tag/0.22.62)
仅用于模拟器/开发环境安装，非正式发行版；物理 PC、DPI 缩放与输入法未覆盖。

## 目录结构

```
app/                   HarmonyOS 应用工程（entry: ArkTS 页面 + C++ 原生库）
native/                适配层 C++：ArkUI 宿主、N-API、原版窗口/命令/对话框
patches/               122 份上游补丁（source.lock.json 记录每份 SHA-256）
cmake/                 上游 GLU/Core/运行时 构建模块与导出符号表
upstream/              固定的上游 Olex2 源码（OlexSys 许可）
vendor/                cpython / libffi / zlib / libpng / gl4es / glu
scripts/               构建、设备验证、来源核对脚本（目录见 scripts/README.md）
tests/fixtures/        设备验证脚本的输入样例（*-inputs.json）
docs/                  设计与验收文档（索引见 docs/README.md）
reports/               小体量验证收据（设备回归 JSON）
CHANGELOG.md           逐批次变更记录
```

## 构建

前置：DevEco Studio（默认 `/Applications/DevEco-Studio.app`）、OHOS SDK API 24 native 工具链、Python 3.10+、`hdc`。

```bash
# 1) 本机编译原生冒烟测试（host + ctest）
scripts/build-baseline.sh host

# 2) 交叉编译到 OHOS arm64
scripts/build-baseline.sh ohos

# 3) 构建未签名 debug HAP（产物落在 out/gx-application-evaluation/app-builds/<id>/）
scripts/build-app.sh
```

`scripts/build-app.sh` 会把当前工作树复制到临时 stage、注入构建来源清单（`OLEX2-BUILD-INPUTS.json`）后调用 `hvigorw assembleHap`，
并输出 `entry-default-unsigned.hap` 与 `provenance.json`（可用 `build_provenance_ffi_v1.py verify` 复核）。

## 验证

```bash
# 设备回归：安装并驱动模拟器，结果写入 reports/app/<run-id>/
python3 scripts/run-app-smoke.py --target 127.0.0.1:5555 --hap out/app/entry-default-unsigned.hap

# 本地证据一致性检查（不启动应用，只核对已保留的收据）
python3 scripts/check-adaptation-status.py
```

## 文档

- [docs/README.md](docs/README.md) — 全部文档索引（按主题）
- [原版功能适配清单](docs/原版功能适配清单-2026-11-14.md) — 逐项功能状态与证据
- [AGENTS.md](AGENTS.md) — 工程约定（改动流程、证据存放、提交规范）
- [CHANGELOG.md](CHANGELOG.md) — 第 1–83 批开发记录

## 历史与证据

2026-11-17 做过一次结构整理：早期把快照、备份、实验证据（`snapshots/`、`backup/`、`experiments/`、`out/`，合计约 3.2 GB）
直接提交进了工作树，整理时全部移出工作树，**内容仍在 git 历史中**（见 [证据归档](docs/证据归档.md)）。
需要某份旧证据时按路径从历史提交取回，不必恢复整个目录：

```bash
git checkout pre-tidy-2026-11-17 -- experiments/original-document62/cpp442-main-gui-v1/toolbar-actions-v1/validation-v2.json
```

## 许可

上游 Olex2（OlexSys development framework）采用 BSD 三条款式许可，全文见 [LICENSE](LICENSE) 与
[upstream/LICENCE.txt](upstream/LICENCE.txt)：保留版权与免责声明，二进制分发需附声明，不得以 OlexSys 名义背书。

本仓库的适配代码与 122 份补丁按同一许可使用与分发；vendor/ 下 CPython、libffi、zlib、libpng、gl4es、glu
各自保留上游许可（随各自目录内的 LICENSE 文件分发）。
