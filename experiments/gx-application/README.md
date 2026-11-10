# 原图形应用平台连接（隔离评估）

**当前仍未闭环。** 最新 0.22.8 仅构建/本机验证；0.22.5 同一批真实 12 启动重核为 9 通过、3 次 fd 数量失败，不能推广到标准基线。原模拟器重启需要用户在 DevEco 确认软件协议，新候选没有完整目标/UI 资格。详见 `docs/原图形应用入口与图片读取评估-2026-11-09.md`；实现中的检查项不等于全部检查已通过。

本目录不是生产运行时，也不代替 `native/application_runtime.cpp` 的唯一所有者。
生产源码锁与标准包仍为 0.22.0 / 16 补丁；候选只更新同一个测试包，不卸载、不操作其他应用。

## 已有实现

- 原 `TGXApp` 注入禁用的外部 `AGlScene`，真实初始化并销毁原 renderer、graphics file、mouse library 和状态注册；不创建 GL context、字体或载入任何结构数据。
- 将 Win32 默认场景和图片读取分支与可移植路径隔离。原 wx/Win32 分支不被本平台实现替换。
- `ohos_texture_image.cpp` 将自身普通文件有界读入自有编码内存并关闭，再通过公开 `CreateFromData` 调用 ImageKit；字节所有者晚于所有公开对象释放。读取实际 source/pixel 元数据、RGBA8888 / share-memory / 行距，并记下逐对象释放返回值，不宣称 SDK 内部无泄漏。
- `texture_image.cpp` 将**实际 alpha 全为 255** 的像素变为 packed row-major RGB，保留行/列与颜色；对透明图明确返回未支持，不用有损反预乘假装等价。
- `OhosTextureGXApp` 真正覆盖候选 `TGXApp::ReadPlatformTexture`，通过原基类 virtual 调用；这不是原 `LoadTextures` 的完整 GPU 上传验证。
- `gx_texture_lifecycle.cpp` 验证正常合成 PNG、中文路径、正常但不符合原纹理尺寸约束的 PNG、未选择文件的返回状态，以及场景、单例和本进程描述符收尾。
- `gx_platform_evaluation.cpp` 仅是 app 进程里的评估入口。普通命令行无法完整加载目标 ImageKit 的应用运行环境，保留该失败，不复制 SDK 占位库或私有系统库。

## 独立可重跑的 CPU 部件

使用已安装 DevEco 的 CMake：

```sh
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
"$SDK/build-tools/cmake/bin/cmake" -S experiments/gx-application -B out/gx-application-evaluation/platform-host
"$SDK/build-tools/cmake/bin/cmake" --build out/gx-application-evaluation/platform-host --parallel 4
"$SDK/build-tools/cmake/bin/ctest" --test-dir out/gx-application-evaluation/platform-host --output-on-failure
```

交叉编译时指定现有 `ohos.toolchain.cmake`、`arm64-v8a`、`c++_shared`、`ohos-24`，额外生成真实 ImageKit 适配静态库。静态库编译成功不是目标端图像读取成功；需要正常 app 进程中的实际结果。

## 构建与证据

完整候选镜像及真实构建配方保留在 `out/gx-application-evaluation/`：

- `source8/`：当前 18 补丁候选源码，独立候选锁，不是生产锁；`source2`～`source7` 历史/中间态仍保留。
- `build-app-candidate.sh`：实际构建和 schema 2 来源留存配方；通过 `GX_APP_SOURCE` 明确选择镜像。
- `app-builds/`：每次包、源码归档、真正编译/链接/依赖、成功和失败 stage。
- `replay-candidate-lock.py`：从原固定提交逐字节正反重放候选补丁，不修改上游目录。
- `scripts/collect-gx-platform-evaluation.py`：读取一次完成的 194 项界面报告，重新核对六轮原绘图原始证据，再仅拉取该报告中明确 PID 的合成图片和结果，核对目标/本地 SHA。

不能将构建纳入比例、注册数、合成数据模型或原窗口有限图元验证写成功能完成百分比。`functional_completion_percent=null`。

## 二维传输独立检查

第 18 补丁已实际编译进候选核心，紧密二维 pack/unpack 的 alignment/row length/skips 在作用域后恢复；替换保留原 ID 和 sampler。真实上游方法的本机接口明确模拟，32 + 409 项检查不是 GPU 证据，见 `experiments/gx-texture-transfer/README.md`。模拟接口未进入 HAP。

## 明确未完成

本目录原ImageKit透明PNG读取（新PNG后端见独立实验）；原 `LoadTextures` 真实窗口纹理上传和状态；字体；持久原 renderer 与现有运行时所有者的正式连接；完整原界面与功能、资源、Python/插件；物理键鼠、真机、签名发行。不要为验证而同时构造两个应用单例，也不要放宽旧 187/新增 7 项检查。

### 下一步场景连接注意事项（尚未实现）

原 `AGlScene::CreateFont` 是延迟注册，原 `TGlFont::CreateHershey` 需要真实当前 GL context 并创建 display lists；原 wx 路径已有 `@` 矢量字体分支，可作为有限文本路线继续验证，但不能据此宣称完整字体/中文支持。现有原窗口验证会在每轮结束关闭 GL4ES；正式持久 renderer、字体和纹理不能在该关闭之后继续复用。先解决上下文拥有权、surface 销毁时的资源释放和重建，再升级生产应用所有者，不能只把基类名字改成 `TGXApp`。

## 后续 PNG 保真候选（2026-11-09）

本目录原 ImageKit 不透明路径及其历史结果保持不变。新的完整PNG解码库在 `../gx-png-image/`，由隔离0.22.9候选接入原virtual seam；56张图本机及模拟器固定3次CPU检查通过，HAP自身12/12启动通过。0.22.9自己的194/756 UI和本轮全部7个明确PID的平台产物核对也已通过，没有借用0.22.8的成功。新虚拟接口须显式选择 `--png-rgb-backend`，不放宽旧ImageKit验证器；原LoadTextures真GPU、1D、字体和持久运行时仍未完成。最新资格以现役报告绑定的真实文件为准。

## 当前一维窗口候选（2026-11-09）

现役0.22.11在 `../gx-texture-1d/`，已取得自己的200 UI、六轮×126原窗口+六轮×64一维窗口、全部7个UI PID收集及12/12启动证据。新0019补丁和限定GL4ES读取适配不改vendor。原194项及其原图元参考保持不变；一维新证据来自实际窗口色条，不是830项本机模拟GL断言。重建仅同context，原LoadTextures真GPU、字体和持久所有者仍未完成。0.22.9段落只描述其历史资格，不代表当前新增一维资格。完整事实与路径见现役机器报告。
