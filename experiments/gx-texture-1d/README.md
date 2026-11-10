# 一维纹理适配实验（独立候选，不是完整渲染替换）

生产 0.22.0、冻结的 PNG 候选 0.22.9 和它们的验证结果不变。本实验仅使用合成颜色图，不读取结构文件或执行研究计算。

## 代码路径

- 第 0019 补丁修正原 `TTextureManager::Add1DTexture` 的生成、绑定和上传顺序，使用真正的一维目标，并恢复调用者的紧密像素传输状态。
- `WriteData` 按纹理类型选择一维/二维接口，而不是以一维读取返回的 `height=1` 误判为二维。
- 资源重建后重建管理器 ID 索引；原纹理对象、名称和配置仍保留。
- 固定 GL4ES 的 `texture_read.c` 对一维 `GetTexImage` 直接返回。只在本候选核心启用 `OLEX2_HARMONY_GL4ES_TEXTURE_1D_READBACK`：通过该固定兼容层的公开二维别名读取同一对象，作用域内保存并恢复原二维绑定。**不修改 vendor，不将此别名约定泛化到其他 OpenGL 实现。**
- 真实窗口代码在原三种图元之后执行，仍使用现有主 surface 上的临时兼容 context。读取 CPU 缓冲不算 GPU 证明；另保存实际窗口像素和驱动程序状态，由独立验证器按数学色条参考核对。

## 当前证据

0.22.11 已构建并完成六轮真实窗口验证：四种 `width=3/5 × RGB/RGBA`，每种两代资源，颜色和 alpha 均独立逐像素采样验证，重新生成后像素一致，原二维绑定恢复且对象最终释放。增加着色器变体后精确要求三对顶点/片段着色器，生产验证器仍保持原两份要求。

- 本机接口捕获：830 断言；包括宽度 1/3/5、四种初始对齐、原一维/二维混合管理器及三轮资源重建。明确为模拟 GL，未入 HAP。
- 原二维传输实验两项 CTest 通过；标准 18 项 CTest 通过。
- 新窗口验证器23项、选择框就绪8项隔离测试通过；候选原窗口26项及完整证据12项反例通过。558项完整Python、标准329一致性通过。
- 19 补丁正向精确重放、反向恢复等共 143 项通过。
- 完整200/200界面、六轮各126原窗口+64一维窗口、该轮全部7个UI PID平台收集和固定12/12启动均通过。UI：`out/gx-application-evaluation/source11/reports/app/20261109-225628-1ef641/result.json`；收集：`reports/emulator/gx-texture1d-platform-app-20261109-230906-87460b18/result.json`；启动：`reports/emulator/gx-startup-audit-20261109-231011-2c44a583/result.json`。全部绑定同一HAP SHA `0107a0f6bf84d975bfd6d1ddc105922a78c73e5a9ea892c5256f46b2445c82dc`。

首次 0.22.10 的一维读取失败、0.22.11 首次界面检查仍要求旧着色器数量的失败，以及后续缺失宿主目录参考文件、选择框未聚焦的失败均保留。选择框现在等待精确导出URI可见或实际聚焦，不删弱原文件名/URI/字节和设置恢复检查。修复或成功不得覆盖它们。

## 重现

```sh
python3 scripts/prepare-texture-1d-candidate.py \
  --base out/gx-application-evaluation/source9 \
  --output out/gx-application-evaluation/<新的独立镜像> --version 0.22.11
```

生成器复制已有宿主目录参考作为界面比较输入，不复制旧模拟器证据。新 HAP 必须使用自己的完整界面报告、全部明确 PID 的平台收集及固定启动审计。扩展平台收集使用 `collect-gx-texture1d-evaluation.py`，普通 PNG 及生产收集器保持不变。

## 未完成边界

这里的资源重建发生在**同一个当前 context**，不是实际跨 context 迁移。只验证 level 0、border 0 的正常 RGB/RGBA 合成纹理；未认证其他 mipmap/格式/全部采样组合。长期所有者仍是 `PlatformApp : TXApp`，没有升级成持久 TGX 应用。原 `TGXApp::LoadTextures` 真 GPU 路径、字体、持久 scene/context/renderer、完整原 UI/资源/Python/插件以及真机和签名发行均未完成；功能完成比例保持 null。
