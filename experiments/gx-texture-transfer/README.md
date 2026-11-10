# 二维纹理传输收尾（本机隔离验证）

本目录执行修改后的真实上游 `TTextureManager` / `TGlTexture` 方法，但图形接口是**明确的本机模拟接口**。不创建 context，不访问 GPU，不把这些替代符号打入 HAP，也不声称完整纹理加载已经可用。

## 实际改动

第 18 候选补丁保存在 `experiments/gx-application/patches/0018-texture-transfer-alignment-and-state.patch`，当前实际镜像为 `out/gx-application-evaluation/source8/`，生产上游及 16 补丁锁未变。

- packed 二维读写时临时设置 alignment=1、row length/skip rows/skip pixels=0，结束后恢复全部调用方原值。
- 替换已有二维纹理时不先删除原对象，保留 ID、名称管理与 sampler。
- 当前只允许的 RGB/RGBA 格式在分配或修改对象之前验证。
- 真实图形绑定语义保留，未声称恢复 active unit、binding、所有 GL 状态或实现 1D 创建。

## 本机复现

```sh
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
"$SDK/build-tools/cmake/bin/cmake" -S experiments/gx-texture-transfer -B out/gx-application-evaluation/texture-transfer-host-final \
  -DOLEX2_TEXTURE_SOURCE="$PWD/out/gx-application-evaluation/source8/upstream"
"$SDK/build-tools/cmake/bin/cmake" --build out/gx-application-evaluation/texture-transfer-host-final --parallel 4
"$SDK/build-tools/cmake/bin/ctest" --test-dir out/gx-application-evaluation/texture-transfer-host-final --output-on-failure
```

`pixel_store_scope` 的 32 项包含嵌套和调用方异常收尾；`original_texture_manager_mocked` 的 409 项调用真实新增/替换/读写/SetCurrent/销毁方法。输入宽度 1、2、4，原 alignment 为 1、2、4、8，并保留非零原 row length/skips。模拟接口在检查行布局正确后才看字节，不重放旧布局问题，不尝试触发驱动故障。

报告 `reports/gx-texture-transfer-host-status.json` 保留真实二进制、源码、编译命令及补丁摘要。当前候选 HAP 的来源另行证明这份实际上游代码和状态头进入 ARM64 构建；**本机模拟调用不是 OHOS GPU 证据**。

布局参数含义参考 Khronos 原始 glPixelStore 参考页：`https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPixelStore.xhtml`；实际固定 GL4ES 的 getter/pixel-store 实现也逐项核对。第三方源码没有本地改动。
