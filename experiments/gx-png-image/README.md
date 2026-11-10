# 原纹理 PNG RGB 保真候选

这是独立的、真实源码构建的 CPU PNG 后端，不是完整应用或渲染器替换。

## 范围

- 固定、原样构建 libpng `v1.6.59` / 提交 `cd952f49f95bb27154ae77dbb103032d95f6e580`，以及 zlib `v1.4` / 提交 `99623ef7097352da7993ba614161fd6a311e046e`。
- `vendor.lock.json` 对全部 885 个原始文件逐项锁定，未修改第三方源码，未使用系统/SDK 预编译 PNG/zlib 库。原始下载归档保留在工程 `out/gx-application-evaluation/png-dependencies/`。
- 读取自有普通文件到有界内存后先关闭文件，再执行 PNG 解码；沿用既有的文件/缓冲生命周期记录。公开 ImageKit 句柄数量为 0，不把这条路线说成 ImageKit 测试。
- 使用完整 libpng 低层读取；扩展调色板、灰度和 tRNS，灰度转 RGB，16 位取高字节，支持 Adam7。去掉 alpha，不做预乘、反预乘、背景合成或 gamma 变换，保留 alpha=0 下的原始 RGB。
- 对照资料为 wxWidgets `v3.2.8` 的 `src/common/imagpng.cpp` 中 PNG 读取与 `CopyDataFromPNG`：仅对照上述 RGB 样本转换，**没有证明原 Olex2 所用 wx 版本及完整 wx 行为等价**。参考文件保留在 `out/gx-application-evaluation/png-references/`。
- 保持原应用所需的正方形、二次幂纹理尺寸及现有 4096 上限；输出行优先 RGB，由原 `TGXApp::LoadTextures` 自行保持历史 x/y 约定。本后端不上传 GL 纹理。
- libpng 的 setjmp/longjmp 仅存在于 C 实现，修改后的状态位于堆中，不跨越 C++ 对象析构。所有 libpng 分配回调及释放回调单独记录；这不证明整个进程或全部系统库资源释放。

## 测试

正常合成图矩阵有 56 张：灰度 1/2/4/8/16 位、调色板 1/2/4/8 位、RGB/RGBA/灰度 alpha 的 8/16 位、tRNS、无交错与 Adam7、1/2/4 边长、中文路径及 gAMA。34 张包含透明数据，29 张为 Adam7。逐字节比对独立构造的预期 RGB，包括透明像素下的颜色。

另测试正常但不符合纹理尺寸要求的 3×3 和 2×4 图片，以及未选择文件的返回状态。不是漏洞复现、崩溃测试或 fuzzing。

- 本机 CTest：624 项断言，56 张均通过。
- 鸿蒙 PC 模拟器独立 CPU 程序：连续固定 3 次，每次 624 项、56 张通过。目标程序、C++ 运行库及每份观察文件核对了目标/本地 SHA。
- 每次有 467 对 libpng 分配/释放回调，已知自有文件及缓冲作用域通过。完整进程文件描述符元数据扫描仍未通过，不调整权限，也不将该限制改写成通过。
- 结果：`reports/emulator/png-texture-cpu-20261109-214942-8d2e22b4/result.json`。

本机构建：

```sh
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
"$SDK/build-tools/cmake/bin/cmake" -S experiments/gx-png-image -B out/gx-application-evaluation/png-host-check
"$SDK/build-tools/cmake/bin/cmake" --build out/gx-application-evaluation/png-host-check --parallel 4
"$SDK/build-tools/cmake/bin/ctest" --test-dir out/gx-application-evaluation/png-host-check --output-on-failure
python3 scripts/vendor_sources.py --root experiments/gx-png-image
```

交叉构建额外指定现有 `ohos.toolchain.cmake`、`arm64-v8a`、`c++_shared`、`ohos-24`；随后用 `scripts/run-png-texture-smoke.py --target 127.0.0.1:5555 --build <实际构建目录>` 执行。该程序不操作其他应用、不开图形上下文，也不安装 HAP。

## 应用接入与未完成项

`prepare-png-texture-candidate.py` 在新隔离镜像中接入 `PngTextureGXApp`，保持同一真实 `TGXApp` 虚拟接口与独占生命周期；不会同时创建生产单例。保留原 ImageKit 能力观察，不把旧透明失真结果改为通过。新后端必须显式使用 `--png-rgb-backend` 验证。

0.22.9候选包自己的来源核验、12次启动、完整194/756界面回归和本轮全部7个明确PID的平台收集已分别通过。各自结果绑定同一HAP SHA `b2f419b0d219ed24eb14708bdae720ff994054c914a84c715dfbabe4e709b744`，详见现役报告。独立CPU测试不替代这些验收，也不能证明 `LoadTextures` 的真实 GPU 上传、完整 GL 状态恢复、1D、字体、持久场景/唯一运行时或完整 P1～P8。

首次本机构建的导出配置错误、第二次配置头误取系统 zlib、首次交叉配置生成缺少目标参数，以及首次候选来源验证未纳入两个构建生成头的失败均保留，没有删除旧失败或放松原始输入字节检查。
