# scripts 说明

所有脚本以仓库根为基准（`Path(__file__).resolve().parents[1]`），构建产物写 `out/`，验证收据写 `reports/`。
依赖 DevEco Studio 自带 SDK（默认 `/Applications/DevEco-Studio.app`）与 `hdc`；设备地址一律用 `--target` 显式传入，脚本不猜设备。

## 构建与打包

| 脚本 | 作用 |
|---|---|
| `build-baseline.sh` | 用 SDK 工具链编译原生层：`host` 跑本机冒烟 + ctest，`ohos` 交叉编译 arm64 |
| `build-app.sh` | 主入口：复制工作树到临时 stage、注入构建来源清单、`hvigorw assembleHap` 出 debug HAP + provenance |
| `build-app-candidate-ffi-v1.sh` | candidate 构建配方（FFI 变体），由 `build-app.sh` 复制进 stage 作为真实配方 |
| `build-app-candidate-python.sh` `-v2` | candidate 构建配方（Python 运行时变体） |
| `build-python-ohos-v6.sh` | 用 OHOS 编译器构建固定版本 CPython（不改上游源码） |
| `package-python-runtime-v1.py` | 把构建好的 OHOS Python 打包进 `vendor/` |
| `generate-python-runtime32-exports.py` | 由 OHOS 探针与 CPython ELF 生成显式 ABI 导出名 |
| `prepare-python-runtime32-v2.py` `-v3.py` | 历史脚本：准备 source32 的 CPython 输入（依赖已移出工作树的快照，仅作参考） |

## 来源与构建追溯

| 脚本 | 作用 |
|---|---|
| `build_provenance.py` | 记录构建快照、原生输入与 HAP 字节的绑定关系，并做回验 |
| `build_provenance_ffi_v1.py` | candidate 配方用的 schema 2 扩展（被 `build-app.sh` 调用） |
| `build_provenance_graphics.py` `_png.py` `_python.py` `_python_v2.py` | 各构建变体的 provenance 扩展 |
| `check-source.py` | 核对上游 commit、补丁集合、复用头文件与许可证 |
| `vendor_sources.py` | 核对 `vendor/` 固定依赖的哈希（不联网、不安装） |
| `check-helper-extraction.py` | 比对抽取出的共有循环体与固定 `glutil.h` |
| `check-graphics.py` | 检查 SDK 链接兼容性（不创建图形上下文） |
| `check-app-package.py` | 检查本地 debug HAP 与设备测试证据是否一致 |
| `report-port-coverage.py` | 统计进入 OHOS 构建的上游 C++ 翻译单元数量 |
| `report_artifact_paths.py` | 纯路径规划工具，不构建、不改证据 |

## 设备验证与回归

| 脚本 | 作用 |
|---|---|
| `run-app-smoke.py` | 在指定模拟器安装并驱动应用，逐项检查并写 `reports/app/<run-id>/` |
| `run-device-smoke.py` | 在指定 OHOS 目标上跑工具层与几何适配器 |
| `run-light-defaults-smoke.py` | 只跑 CPU 光照默认值测试 |
| `original_renderer_click.py` `_receive.py` `_capture.py` | 原版渲染器窗口的点击注入、接收与截图采集 |
| `original_renderer_validation.py` | 校验共享窗口的合成渲染与宿主连续性 |
| `original_renderer_integration_validation.py` | 复核已保留的真实窗口证据（不据此宣称整体完成） |
| `test-original-renderer-validation.py` | 校验脚本自身的负例固件（不改真实证据） |
| `write-original-renderer-integration-status.py` | 复核通过后写出限定的集成摘要 |
| `picker_readiness.py` | 发文本前先确认系统选择器就绪，不做绕过 |
| `app_report_log.py` | 解析应用运行记录（要求片段完整一致） |
| `sync-verified-reports.py` | 从已通过的证据同步当前 0.22 摘要 |
| `check-adaptation-status.py` | 本地证据一致性检查（不启动应用、不认证整体完成） |

## 验证断言库

被上面的驱动 import，用来表达"什么算通过"：

| 脚本 | 作用 |
|---|---|
| `layout_validation.py` | 控件可见边界必须等于原始未裁剪边界 |
| `line_validation.py` | 线段索引契约（不执行原生代码） |
| `line_path_validation.py` | 固定拓扑、EBO 字节与合成边缘像素 |
| `polygon_validation.py` | 多边形来源与隔离 FBO 像素 |
| `matrix_validation.py` | 相机状态与 GPU 读回的解析校核 |
| `texture_1d_window_validation.py` | 真实窗口内 1D 纹理取样检查 |
| `desktop_shell_validation.py` | 安装身份与受限桌面输入检查 |
| `desktop_command_validation.py` | `pc.*` 命令样例的固定期望 |
| `options_document_validation.py` | 系统选择器/设置对话框的固定期望 |
| `original_log_validation.py` | 原版日志通知的字节与计数核对 |
| `application_runtime_validation.py` | 应用运行时与固定 UI/设置期望 |
| `renderer_probe_identity.py` | 探针身份必须来自保留的包，而非运行中的应用 |

## tests

`tests/` 是上述断言库的 pytest 用例：

```bash
python3 -m pytest scripts/tests
```
