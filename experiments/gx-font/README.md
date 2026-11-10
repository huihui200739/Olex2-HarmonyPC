# 鸿蒙原生字体接入（有限真实窗口资格，未完成完整字体适配）

## 当前开发候选：0.22.21 原矢量字体接入（覆盖前述19当前状态，2026-11-10）

source19 / 0.22.19的212/7PID/12启动/18交替动作是上一份完整诊断资格，已归档，不能继承到21。
当前安装为source21 / 0.22.21，HAP
`out/gx-application-evaluation/app-builds/20261110-033149-8526e4/entry-default-unsigned.hap`，
SHA `a6e2a69e1c5b84b1249ef77af9515f5d9a9b37d9d376344c1995ddf8177b7131`。
609实际编译单元、379上游cpp、20补丁、367明确导出、3库来源通过；这些不是功能完成率，标准0.22.0不改。

source20真实接通原`@18`/CreateHershey/DrawVectorText；保留原始95字形表与原metrics初始化/间距语义，
不将vector偷偷替换为NativeDrawing位图。`@`默认15、`@<1..128>`明确支持；其他旧描述/资源仍未完成。
原场景缩放不改变矢量字体固有点字号，draw scale才改变其几何显示大小；原native位图18→27→18检查完整保留。

20首轮实际失败：8个新颜色写入布尔状态检查未过；其像素/metrics/缩放比对已通过，不能改写原失败为成功。
21只改父库状态读取和版本两文件，保留原布尔值，额外读取原始符号布尔值与GLint状态，不改vendor/SDK或绘图状态。
21三轮成功：PID见原报告，210独立检查、2次真实context连续恢复；24份实际整数状态全1，原布尔差异原样保留。
只断言已观察到查询差异，不归因驱动内部根因。位图9幅、矢量12实际幅/12直接几何参考幅比对严格通过。
矢量参考直接重放固定上游几何且不调用TGlFont，但共用GL4ES/GPU，**不是独立GPU线条光栅器证明**。

现役`reports/gx-vector-font-status.json`与另外5份GX/font报告相同。
原报告 `reports/emulator/vector-font-state-window-20261110-033453-61ef788e/result.json`，
独立重读原始像素/metrics/布局/当前PID/文件摘要/实际DSO反汇编见
`reports/emulator/vector-font-state-revalidation-20261110-033901-2e51b48d/result.json`。
20/21原生输入、构建和失败保持冻结。首次解析器误限负坐标的本机失败保留，按真实原表的下伸负坐标修正；没有改表或放宽像素比较。

当前完整885 Python（先前863+新增22）通过；未改字体组件原3 CTest资格保持，不声称重新设备执行；7584字体描述/字号隔离断言不含NativeDrawing/GL/设备执行，未入HAP。
21生成器2513文件精确重放，21 UI工具65配方文件+3元数据共68文件重放通过；先前67计数漏了宿主日志，旧统计保留。
**21自身212/212诊断UI、全部7个UI PID资源、固定12/12真实初始化启动已串行通过，并独立重读原始证据。**
UI报告：`out/gx-application-evaluation/vector-font-state-ui-tools21/reports/app/20261110-033715-b2b628/result.json`。
资源报告：`reports/emulator/gx-vector-font-state-platform-app-20261110-040809-923b0c39/result.json`；
启动报告：`reports/emulator/vector-font-state-startup-audit-20261110-040855-1481e445/result.json`，保留120项真正长期owner初始化检查。
`vector_font_state_current_qualification.py`重新验证7PID的全部固定PNG/Unicode文件字节、原始资源观测、12份runtime/显式PID日志、执行profile和5个依赖；不只相信passed字段。
公开资源释放/自有内存资格与完整进程FD元数据证明严格分开，后者实际仍为false，不声称SDK内部所有分配已释放。
新增26项纯合成审计/参数隔离测试通过，当前完整885 Python回归通过（含新增20三路隔离+2记录器参数检查）；本机模型不冒充设备观测。
本轮合并复核：`reports/emulator/vector-font-state-revalidation-20261110-042845-8f1dfdc4/result.json`；6份现役汇总相同，并保留先前字体/UI/7PID/12启动资格的原始独立重验，整体仍in_progress/false/null。
**21自身三路交替已首轮实测通过并独立重验：2个实际PID、18次动作，覆盖全部6种有向切换。**
原报告：`reports/emulator/vector-font-state-mixed-20261110-042347-44ff22e8/result.json`。
各路线局部run仍为1/2/3，全路线context租约严格为1～9；每字体动作6次真实请求，各PID最终18次闭合。
保留402项完整native/vector字体检查、24幅矢量实际/24幅同GPU几何参考、48份实际整数mask快照和186项跨路线检查；检查类别不相加冒充功能覆盖率。
加载路线仍无直接字体计数，末尾真实字体动作只证明跨边界累计闭合，不声称加载本身直接观测。
全部21关键配方及生成的当前PID采集配方归档核验；原单路线font_requests=0、19的3次profile与21单字体profile均未修改。

完整字体资源/字体族样式选择/对话框/DPI/广泛字符、完整原UI/菜单/命令/文档/资源/Python/插件和P1～P8仍缺。
真机/物理输入/正式签名发行未验证；整体in_progress/false/null。跨对话消息条件未满足，未发送。
详见`docs/原矢量字体与显卡状态读取-2026-11-10.md`。


## 当前实际接入

source19 / 0.22.19已接通PlatformWindowScene字体创建、缩放/恢复，
NativeDrawing → 共同基线256个RGB单元 → 原CharFromRGBArray/CreateGlyphsFromRGBArray
→ 原DrawRasterText。没有替换上游字体算法，没有另造应用单例。
原字形对象跨真实context保留，旧显示列表在close_gl4es之前退役，下一context重建。

当前只接受显式`ohos-native:<point-size>:<r|b|i|bi>[:fixed]`描述，换算固定96 DPI。
CPU缓存、异常借用指针清理和unpack alignment恢复已接入；默认粗/斜体代码路径存在，
但当前真实窗口资格仅覆盖常规ASCII `Ab09`。不能据此宣称其他样式、固定宽度、字体选择或Unicode全部通过。

## 原始证据与独立复核

- 三轮真实窗口：`reports/emulator/native-font-raster-window-20261110-024306-ff62b82f/result.json`。
  PID10798，正常/缩放/恢复各三次，两次实际context衔接，85项检查。
- 独立重新读取原始RGB、整幅RGBA、布局、当前PID捕获和逐文件SHA，见`reports/gx-font-status.json`。
- HAP SHA `746ee8334421c2ebba937f646dc8d6b869fdb9c494dd1bc292a66732e9831f13`。
- 保留原整数栅格位置，参考计算由实际矩阵和实际ARM64 FMLA精度得到，
  不是图片拟合偏移；实际构建DSO指令再次反汇编核对，证据在
  `out/gx-application-evaluation/native-font19-raster-binary-proof.json`。
- 独立本机2 CTest分别覆盖单元打包与三路计数，不调用目标NativeDrawing/GPU，不进入HAP。

## 保留失败和未完成范围

17因工具栏增加后原控件裁切而终止，未点击字体；18布局修复后，
像素形状/颜色/数量一致但理想实数坐标参考与实际float32运算不符而失败。
19没有移动原坐标或放宽门禁，只记录真实矩阵并使用实际二进制运算模型。
三份候选、失败报告及参考/实际图片均保留。

19自己的212/212诊断UI、全部7个UI PID资源和固定12/12启动已通过且重新核对原始证据。
2PID/18动作的字体/原绘图/加载双向交替也通过，覆盖六种有向切换，
保留756原窗口、60生命周期、384临时一维、300长期一维、788加载、152字体及180混合检查。
每入口独立run1/2/3，物理窗口租约全动作1～9；每PID9次字体请求严格闭合。
加载路线没有直接字体计数遥测，准确计数由每个字体/绘图边界和最终字体动作闭合，不能声称已直接观测加载路线计数。
当前完整784 Python、标准18 CTest与字体2 CTest通过；没有把16旧成功继承到19。
完整旧字体描述/资源、字体族与样式选择、字体对话框、真实DPI、广泛字符与原UI集成仍需继续。
不声称完整适配、真机验证、正式发行资格或SDK内部全部无泄漏。

## 历史：接入前的交叉编译准备快照

以下记录只描述接入前状态，不覆盖上面的最新资格；
`reports/gx-font-raster-preparation-status.json`仍保留原快照，不作为当前运行证明。

# 鸿蒙原生字形后端（准备中，未接入当前包）

目标是将本机 SDK 的 NativeDrawing 字形转换为原 TGlFont 可接收的 RGB 数据，
随后接入真正的原字体创建、显示和环境恢复流程，不以另画一套标签代替原字体。

当前实现 `native/harmony_glyph_raster.*`：

- 使用真实 NativeDrawing 字体、UTF-32 单字形测量、文本对象和软件位图；不是宿主字体模拟。
- 像素字号与点/DPI换算分开，保存相对于原基线的位置与 advance；同时纳入测量/文本对象边界。
- 返回顶向下、紧密排列、白底黑字的 RGB；保留空白字形的 advance，不把透明图反预乘。
- 调用方可提供实际选中的字体对象；默认字体、粗体和倾斜分支已有实现，但字体名称/样式选择尚未接入。
- 显式限制字号、Unicode标量、有限边界与位图尺寸；自有像素和公开NativeDrawing对象使用作用域管理。

## 本轮实际证据

目标端静态库及链接检查程序已由已安装 SDK 编译并链接；未运行链接检查程序，
未安装新包或操作模拟器，当前0.22.16 HAP完全不变。

- `out/gx-application-evaluation/gx-font-raster-cross-build-attempt1.log`：初次编译失败保留。
  SDK编译步骤带入未使用的gcc-toolchain参数，与本组件源代码警告按错误处理冲突。
- `out/gx-application-evaluation/gx-font-raster-cross-build-attempt2.log`：只忽略该编译器驱动参数警告，
  实现源代码其他警告仍按错误处理；真实ARM64静态库与NativeDrawing程序链接通过。
- `reports/gx-font-raster-preparation-status.json`：实际源字节、编译命令、链接产物和失败记录绑定。

## 尚未验证/接通

没有宿主伪绘图测试或目标运行/字形像素证据，不能声称字体显示通过或SDK内部无泄漏。
尚未接入 PlatformWindowScene::DoCreateFont、原256字形共同基线/单元拼接、
TGlFont::CharFromRGBArray/CreateGlyphsFromRGBArray、真实窗口显示/缩放和context恢复。
字体族/真实样式选择、旧字体描述/资源格式、字体对话框、DPI与原UI集成也未完成。
不得因此修改当前16的fonts_implemented=false或整体适配完成标志。
