# 工程约定（给人和 AI 协作者）

## 目标与口径

- 目标：原版 Olex2 功能在 HarmonyOS PC（API 24 / arm64）上可运行、可验收；验收口径按用户 2026-11-15 放宽到 **约 80–90% 可运行**，不做上架、正式签名、插件体系与 html 面板。
- 状态词只有三个：`已验证`（原生编译/严格链接 + ArkTS 编译 + 模拟器实际交互与结果核对）、`部分`（已接入且部分场景验证）、`未开始`。
- 442/442 是**源码编译与严格链接资格**，不是功能完成率。整体完成度只以 [docs/原版功能适配清单-2026-11-14.md](docs/原版功能适配清单-2026-11-14.md) 为准。
- 没有设备证据的改动写“已接入未验证”，不写“完成”。

## 目录职责

| 目录 | 放什么 | 不放什么 |
|---|---|---|
| `app/` | HarmonyOS 工程：ArkTS 页面、C++ 原生库、资源 | 构建产物（`build/`、`.hvigor/`、`oh_modules/`） |
| `native/` | 适配层 C++（ArkUI 宿主、N-API、原版窗口/命令接入） | 上游原样文件（放 `upstream/`） |
| `patches/` | 对上游的补丁，编号递增，逐字节可重放 | 随手改的上游文件 |
| `cmake/` | 上游库构建模块、导出符号白名单 | 本地路径 |
| `upstream/` | 固定的上游源码（勿改） | 本地修改（走 `patches/`） |
| `vendor/` | cpython / libffi / zlib / libpng / gl4es / glu | 预编译缓存 |
| `scripts/` | 构建、设备验证、来源核对脚本（见 `scripts/README.md`） | 一次性临时脚本 |
| `tests/fixtures/` | 验证脚本的输入样例 `*-inputs.json` | 运行产物 |
| `docs/` | 设计/验收文档、功能清单 | 截图、日志 |
| `reports/` | 体量小的验证收据 JSON | 整棵源码树、HAP、截图 |
| `out/` | 本地构建与临时证据（已在 .gitignore） | — |

## 改动流程

1. 改 `native/`/`app/`，对上游的必要修改落到 `patches/00NN-*.patch` 并在 `source.lock.json` 登记 SHA-256。
2. 原生编译 + 严格链接（`--no-undefined`）；新增导出符号必须同步 `cmake/original_renderer.exports`。
3. ArkTS 整页编译（0 error；既有告警数不得增加）。
4. 构建未签名 debug HAP，在模拟器上真实点击/注入并读回原生状态，结果写收据。
5. 更新功能清单状态与 `CHANGELOG.md`（写清批次、断言数、限制）。

```bash
scripts/build-baseline.sh host     # 本机原生冒烟 + ctest
scripts/build-baseline.sh ohos     # OHOS arm64 交叉编译
scripts/build-app.sh               # debug HAP + provenance
python3 scripts/run-app-smoke.py --target 127.0.0.1:5555 --hap out/app/entry-default-unsigned.hap
python3 scripts/check-adaptation-status.py
```

## 证据规则

- 小体量 JSON 收据进 `reports/`；截图、日志、整树快照、HAP 留在 `out/` 或本地归档，**不进 git**。
- 证据文件名带运行 ID 或日期，收据里写明 HAP 的 SHA-256 与断言数。
- 失败与回退也要留收据，写清“哪条不过、为什么回退”，不要删掉重来。

## 提交规范

- 一个提交只做一件事；信息格式 `<模块>: <动作>`，例如 `native: 接入原版键长对话框宿主`、`patches: 0105 归一化对称平移`。
- 涉及设备验证时在正文写：批次、构建 ID、断言数、限制。
- 提交前确认 `git status` 里没有构建产物、HAP、密钥、整树副本。
