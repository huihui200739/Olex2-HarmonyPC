# 唯一长期 GX 所有者与窗口租约（进行中，不是完整适配）

## 当前实现

隔离候选 **0.22.13 / source13** 将真正的长期应用从 `PlatformApp : TXApp`
改为 `PlatformApp : PngTextureGXApp : TGXApp`。基本应用、核心、图形应用单例
指向同一个对象；真实 `TGXApp` 所有的 `TGlRenderer` 和 `PlatformWindowScene`
跨多次窗口绘图保留。主画面仍为已有 GLES3 适配，不是完整原绘图接管。

- 进入窗口路径时锁序为 NAPI 所有者 → runtime mutex → 非递归原解释器锁。
  C ABI 入口不再先取原解释器锁后调用会重复取锁的 runtime 方法。
- 场景持有窗口 ABI 的值副本，只在有效表面租约中启用；结束后清空回调并禁用。
- 逐图元清理只清真实绘图器的集合与显示列表，不销毁长期绘图器或场景。
- 清理顺序为图元集合 → `BeforeContextChange` → 场景解绑 → `close_gl4es`；
  外层既有逻辑随后解除临时 EGL 绑定、销毁临时 context、恢复主 context 和下一帧。
- 图元对象存活期由异常清理保护；平台选项配置在场景所有权转移完成后执行，
  避免派生构造函数配置失败时重复释放已被基础绘图器拥有的场景。
- 未实现字体的接口明确失败，不用空实现冒充支持。不调用原 `LoadTextures`，
  因而没有隐瞒、回滚或排除它应产生的真实日志。

## 实际包与目标证据

- HAP：`out/gx-application-evaluation/app-builds/20261109-233724-dede29/entry-default-unsigned.hap`
- SHA256：`c4697aa9334cdb2004dc06b5a6b55634a6746310a55a5f9fcf82ef28f38591a8`
- 包内三份原生库和固定源码来源核验通过；602 实际编译单元、379 上游 cpp、
  19 补丁、367 明确导出只是构建事实，不是功能完成比例。
- 模拟器实际窗口：
  `reports/emulator/persistent-gx-window-20261109-233923-7ce43126/result.json`
- 四个独立 PID：29145、29171、29716、30239；最后 PID 连续 run 1/2/3。
- 六轮各 126 原窗口检查、10 长期生命周期检查、64 一维检查，全部通过。
  同 PID 中应用、绘图器、场景三个不同对象的实际地址保持不变，表面租约计数平衡。
- 原窗口验证器仅显式调整三个已知约束：精确三对 FPE shader、真实 GX 注册数
  193/46、场景每轮销毁数从临时场景的 1 改为长期场景的 0。
  像素/矩阵/驱动状态/主画面下一帧/全部非图形宿主状态相等约束保留。
  **这是明确的新生命周期资格，不宣称与旧销毁语义完全相同。**
- 新记录器重读原始窗口证据，并对每个不可变 PID/run 目录的目标文件重新核对 SHA。
  宿主 `shared-original-integrated-run-1.json` 路径会被后续 PID 覆盖；旧汇总仍要求
  最初接收时的精确目标 SHA，不把它标作现在重新读取的旧进程文件。

## 回归与工具

- 完整 Python：606 项；原标准 host CTest：18 项；全部隔离验证器：48 项。
- 新平台场景状态机：1 个本机 CTest / 83 断言，使用**明确建模的基础场景与回调**，
  不是原场景或 GPU 资格；测试代码未进入 HAP。
- 长期生命周期验证器：14 个测试方法，包含缺字段、伪布尔、计数/地址/源描述/
  跨进程/重复运行等拒绝用例；全部为明确的隔离合成观察。
- 生成器从冻结 source11 再生成 10 个关键文件，与 source13 精确字节相同。
- 源码生成：`scripts/prepare-persistent-gx-candidate.py`，只接受固定 19 补丁的 0.22.11
  基线；现仅允许生成 0.22.13，不覆盖已有目录。
- 目标运行：`scripts/run-persistent-gx-window-smoke.py`，必须显式 target、source、hap。
- 当前结论：`reports/gx-application-platform-status.json` 与
  `reports/gx-persistent-runtime-status.json`；最新 `record-persistent-gx-ui-progress.py`
  独立重核包/完整UI/全部PID平台资源/12启动/宿主目录/本机日志后写入，
  整体 `passed=false`、完成比例 null。

## 版本历史与尚缺项

source12 / 0.22.12 的真实六轮窗口也通过，但保留为先行候选；source13 增加
构造后配置和逐图元异常清理保护。source12 首次采集误读了旧 0.22.11 快照并正确失败，
记录保留。新的采集必须同时匹配安装身份和当前活进程日志中的完整报告，不能只看旧文件存在。
首次记录器把依赖路径误写成不带 `project/` 的路径，失败日志保留，已改为核对精确路径。
第二次记录器因 Python 整型字典键与已保存 JSON 字符串键的差异拒绝了原本通过的观察；
保留失败后，改为对完整值进行禁止 NaN 的 JSON 序列化比较，不删除或忽略任何证据字段。

0.22.13 已取得新包自身的206/206诊断UI、全部7个UI PID平台资源检查与固定12/12启动资格，
不是沿用0.22.11。原200项名称和顺序保留，新增6项长期生命周期汇总。
每次启动另有10项真实长期owner初始化检查，完整进程日志与当前PID快照匹配。

独立真实GX元数据宿主382实际编译单元/379上游cpp、1 CTest/276断言；
原processor执行LstMac/LstFun并与真实注册对象逐字节对照。顶层193宏/46函数，
含子库递归196/52。它不创建GL context、不加载数据，不是GPU验证。
独立UI工具目录 `out/gx-application-evaluation/persistent-gx-ui-tools13-v2` 的64文件已精确重放；
冻结source13与HAP不改，构建来源核验仍指向真正source13。

完整证据路径与仍未完成边界见 `docs/长期GX诊断界面与启动回归-2026-11-10.md`。
后续原LoadTextures真实GPU和真实日志增量、纹理实际跨context迁移、字体、完整原UI/资源/
Python/插件与P1～P8均仍需继续；现有一维manager仍临时，资源重建仍同context。
不得回滚真实日志、排除宿主状态或用同context重建冒充跨context迁移。
