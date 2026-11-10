# Olex2 鸿蒙 PC 适配 — 当前工程规则

## 本轮用户授权的GitHub备份（2026-11-10，优先于历史不提交规则）

- 用户明确要求推送到其 `huihui200739` GitHub账号；本轮仅授权备份本工程到私有 `Olex2-HarmonyPC` 仓库，允许必要的本地初始化、提交和推送。不得推送到账号内其他项目，不上传凭据。
- 完整适配目标仍active/false/null；新进度说明为 `docs/进度汇报与交付边界-2026-11-10.md`。六项部分进展/零项整体验收不等于功能百分比。
- 最新冻结集成源码保持原位置原字节；`snapshots/source28`是原字节/权限备份副本。根目录app/native仍是旧标准基线。
- 1080完整Python与新增17项隔离测试分开记录；尚未重跑新增工具后的完整回归。

## 当前权威状态：已安装28，完成授权旧副本清理（2026-11-10，优先于下方历史）

- 本轮用户明确授权清理已备份旧测试副本。原计划SHA `274134348017125b4411585603efddd771b20a8d6c92b7918d021e11130cfc20`绑定授权，81目录/4502文件逐文件重新核对后已删除；本机备份保留并重验，8473份非候选可读文件SHA未变。清理前后可用533736→1356064 KiB，增加822328 KiB。配置/运行库/失败/25诊断/其他应用均不在清理候选；未遍历受保护runtime内部，只有公开目录元数据比较。
- 普通shell删除权限拒绝（删0文件）已保留，随后使用标准HDC `shell -b com.example.olex2harmonypc`本应用调试目录入口，逐目录核对相同inode等元数据，只删除精确文件及空目录；不使用root/smode/chmod，不改系统权限或身份配置，不通过该入口读取受保护安装代码。不得对后续新诊断或其他目录扩展这次81目录授权。
- 实际已安装source28 / 0.22.28 / 222800。HAP `out/gx-application-evaluation/app-builds/20261110-071832-be5dfc/entry-default-unsigned.hap`，SHA `3c66b1aeda753d2963cfe0e1810d8b3067ec04a7634e0e8bea7e54d8b6c25cb0`；615实际原生单元/2440 staged/1942依赖/3库来源复核通过。source27/28精确2538/2539文件重放通过；source16～28及已执行配方继续冻结，后续原生修改用29等新候选。
- 28自身三轮字体窗口首轮终态0：PID22027，195/202/202共599检查、两次真实context连续恢复、20/40/60实际请求；3 native+4 vector+3 resource+6 resample+5 family/轮。实际目录观察10字体族，不宣称全部验证。报告 `reports/emulator/font-display-window-20261110-074045-37e45d82/result.json`；独立重读 `out/gx-application-evaluation/font-display28-window-offline-revalidation-attempt1/result.json`，同599项逐项一致，不是新设备运行。
- 真实main-window显示器0/revision1，x/y DPI264.1600036621094、density1.899999976158142；float32(points*dpiY/72)、scaleX=dpiX/dpiY已绑定8个native/family case与UI前后快照，不额外乘density。固定显示器有限验证，不是多显示器切换资格；`actual_display_DPI_qualified=false`，不把模型事件测试当真实切屏。
- 新状态 `reports/gx-font-display-candidate-status.json` 优先。7份25汇总保持原字节、仅为历史25资格，不是最新28当前资格。26历史安装失败/旧实际25记录与27未安装记录保留；不转写为28成功。28自身212UI/7PID资源/12启动/混合路线仍未执行，不能继承25。
- 完整1080 Python通过，执行前260份Python配方完成后摘要一致；前轮1063/261混合配方也已真实终态。Node100通过，host CTest1/1及14740明确合成断言。17项清理隔离测试通过，前期失败保留。均不能换算功能完成率。
- 完整字体对话框/广泛字符/切屏/异常恢复、完整原UI/命令/文档/Python/插件、P1～P8及真机/物理输入/正式签名发行仍缺；全目标active/false/null，跨对话消息sent=false。详见 `docs/28显示DPI验证与旧测试副本清理-2026-11-10.md`。


## 历史：候选26与清理授权前的25状态（以下为当时事实，不覆盖28）

- 冻结source26 / 0.22.26，HAP `out/gx-application-evaluation/app-builds/20261110-061914-5d11ae/entry-default-unsigned.hap`，SHA `9c7cd65e80ec45541f16d11edc47238e2490948afa1aac7b2b4ae15508235612`；11新输入、615实际编译单元、3库、2534文件精确重放通过。source16～26及已执行配方保持冻结，后续原生修改用27等新候选。
- 系统字体族/原始SDK样式精确选择已实现但未在应用中执行；真实DPI仍固定96换算，ShowFontDialog未实现。不能将源码或本机模型当作真实字体目录/窗口资格。
- 安装真实失败9568288（空间不足）；HDC退出0不等于成功。原26运行器/失败保留，新严格 `run-font-family-window-smoke-v2.py` 尚未执行，不得在空间未改变时重复安装。
- 已只读重查实际安装仍25；7份25现役汇总和全部25有限资格不变，不转记给26。新候选单独记录 `reports/gx-font-family-candidate-status.json`，应用字体0轮，没有新UI/启动/混合资格。
- shell探针因NativeDrawing传递依赖加载失败，无实际字体结果；自有临时目录清理已确认。不要重试相同shell加载或修改系统/SDK链接器环境。
- 只读备份审计 `out/gx-application-evaluation/diagnostic-backup-audit-old-success-attempt1/`：81旧成功目录/4502文件/826026629字节，当前设备SHA及完整集合等于本机备份，无失败或25交集。`reports/pending-diagnostic-cleanup.json` 为待授权清单，**未授权、未删除**；不可将继续适配等同于清理授权。保留配置/runtime-v1/失败/现役25/其他应用及本机备份。未来获准仍须重新核对当前字节和活跃PID。
- 当前完整1049 Python通过，250份执行配方冻结摘要核对；22项备份审计测试通过。来源独立重验615单元/2437 staged/1941依赖/3库通过。均不是26设备资格。
- 全目标in_progress/false/null，跨对话消息sent=false；详细说明 `docs/26系统字体族候选与空间阻碍-2026-11-10.md`。

## 历史：25原字体资源与高质量缩放的有限资格（非当前安装）

- 当前冻结source25 / 0.22.25已安装，HAP SHA `2790058639f13787a4b6c7aaab5123978bc38d7a26ca2bbf9e50468042759cdf`，独占构建 `out/gx-application-evaluation/app-builds/20261110-051923-e2a0bc/`。613实际编译单元、21候选补丁、367明确导出、3库来源；标准0.22.0/16补丁不改。
- 现役合并独立复核 `reports/emulator/font-raster-flush-mixed-revalidation-20261110-060136-da56fb5c/result.json`；原有限字体独立复核 `reports/emulator/font-raster-flush-revalidation-20261110-052707-2aa00147/result.json` 保留。7份GX/font现役JSON相同，整体in_progress/false/null；21/23资格均保留在history，不能继承旧UI到25。
- 25三轮窗口已首轮终态退出0：PID12251仅为本轮观察，原报告 `reports/emulator/font-raster-flush-window-20261110-052156-cf138aae/result.json`。445独立检查、两次真实context重建；每字体动作15次请求，三轮最终45次。18幅新增resample实际画面/256字形RGB与固定wx源码方法逐字节一致，9幅原resource画面、native/vector门禁保留；51份vector/resource/resample实际整数mask全1。
- 新资源格式 `#file:style-size` 真正通过原CreateGlyphs；范围外原8/72资源选择通过RGB高质量重采样再走真正CreateGlyphsFromRGBArray。设备新增7、31、46、31、96、72字号及nr__/nrb_/fib_/ni__合成样式；不扩大成全部8样式、全部字号或完整字体族资格。
- 原始24失败保留：`reports/emulator/olex-font-resample-window-20261110-051354-68f22583/result.json`，一个shader前缀误匹配及固定宽度字形交界实际4像素缺失。25的0021只在平台宏下让原DrawRasterChar逐字形flush，保持glyph/metrics/位置和95矢量表；整幅参考仍是透明位图叠加，不迎合GL4ES错误擦除，不平移/加容差/改vendor。宏及实际DSO分支证明 `out/gx-application-evaluation/font-raster-flush25-glyph-flush-proof.json`。
- 22真实构建失败及source22冻结；23原资源三轮287检查已独立复核。source16～25及HAP/配方均冻结；后续原生修改新建26等候选。source25继承的vector20格式版本保留，只通过显式25 overlay重绑新的原font.cpp摘要，旧source20/21不改。
- 完整1005 Python、2个host CTest/1994合成断言/6410组657354字节固定wx3.2.6源码方法对照通过；仅最小存储shim，不冒称完整wx运行库或GPU。wx参考代码仅本机测试，不进HAP；重采样编译显式`-ffp-contract=off`。来源与严格双精度方法模型不代表所有历史wx版本的二进制资格。
- 24/25与新fixture精确重放5/2526/2528文件；21补丁对29文件精确正反重放、vendor原字节核对通过：`out/gx-application-evaluation/font-raster-flush25-replay-attempt1/result.json`。完整901/921/925的各轮日志与失败均保留。
- **25自身212/212诊断UI、全部7个UI PID资源及固定12/12真实初始化启动均首轮通过并独立重读原始证据。** UI原报告 `out/gx-application-evaluation/font-raster-flush-ui-tools25/reports/app/20261110-053551-c4b620/result.json`；资源 `reports/emulator/gx-font-raster-flush-platform-app-20261110-055308-86d27349/result.json`；启动 `reports/emulator/font-raster-flush-startup-audit-20261110-055404-db11951b/result.json`。保留212全部门禁、7PID所有固定PNG/Unicode字节、12份当前PID日志及120项真实owner初始化，5个实际执行依赖精确留档。完整FD元数据扫描仍false，不宣称SDK内部所有分配释放。25工具65配方+3元数据共68文件精确重放；普通路线font_requests=0及旧0/3/6/9请求guard均未改。
- 字体族/原对话框/DPI/广泛字符/异常恢复、完整原UI/命令/文档/Python/插件与P1～P8仍缺。部分FD元数据拒绝的历史保留，25本轮完整FD元数据扫描实际仍为false；不宣称SDK内部全部分配释放或安装受保护代码已直接读SHA。仅合成图形/字体与空应用；未加载结构数据或进行研究计算。
- **25自身三路混合已首轮通过并独立重读原始证据**：实际PID18090/18137、18动作、全部6种有向切换；每字体动作15请求、各PID末尾实际45请求。原报告 `reports/emulator/font-raster-flush-mixed-20261110-055555-6827980d/result.json`；保留872项完整native/vector/resource/resample字体检查、102份实际整数mask、24幅vector实际/24幅共GPU参考、18幅resource/36幅resample实际画面、192项跨路线检查，以及原绘图756/生命周期60/临时一维384/持久一维300/加载788项实际检查；类别不相加冒充覆盖率。加载路线没有直接font计数，边界/最终动作只证明累计闭合。26配方与实际当前PID采集profile均归档精确核对。所有本轮exec64561/86352/23097/84503/63603/76030/63793均成功终态；当前没有正在运行的本轮模拟器测试，不能无故重跑。7份现役汇总相同，1929份独立绑定证据及1378份混合目标/本机接收字节核对见 `out/gx-application-evaluation/font-raster-flush25-mixed-final-consistency.json`。全目标仍in_progress/false/null，跨对话消息sent=false。下一项是字体族/原对话框/DPI等真实功能缺口；原生修改须生成新26候选，标准及source16～25均保持冻结。详见 `docs/25自身界面与三路交替验证-2026-11-10.md`。


## 范围与安全操作

- 本对话只修改本目录，不修改父目录、其他项目、已有文章、配图或投稿记录。
- 不修改全局 SDK/系统设置，不安装全局依赖；使用已安装 DevEco。用户没有鸿蒙 PC 真机，继续本机、交叉编译和模拟器开发，不反复索要真机。
- HDC 必须指定明确目标；本次已用 `127.0.0.1:5555`。不 reset、不卸载；应用回归只更新/重启本验证包。
- 保留旧包、实际失败和运行证据。不要因一条路线通过而覆盖另一条失败；不提交/push 或创建新 worktree。

## 源码、构建与来源

- `upstream/` 固定于 `source.lock.json` 的真实提交；变动必须保留许可、最小 `patches/`、锁中摘要和精确基线重放。
- `vendor/` 由 `vendor.lock.json` 固定；第三方源码原样构建，不做本地源码修改或向其上游贡献。本工程外围兼容代码不属于第三方源码补丁。
- `python3 scripts/check-source.py` 当前应验证 16 个补丁；0015 显式补纹理头 GL 接口，0016 将原 `short SpotExponent` 初始化为 0，不改变其类型或其他灯光默认值。
- `python3 scripts/vendor_sources.py` 必须核对精确文件集合和原始字节。GL4ES 的 `spec/build` 是源码文件，不可用应用生成目录过滤器将其排除。
- `./scripts/build-app.sh` 使用独占英文临时快照，不能重命名用户目录或修改 Hvigor/SDK。成功包、未剥离输出、源码/配方/锁的原字节、真实 C/C++ 命令与 Ninja 依赖保存到独占构建目录；后续失败快照保留。
- schema 2 来源覆盖全部 3 份包内库和固定 vendor；schema 1 继续兼容旧包。来源追溯不认证签名、全 SDK、可重复字节或完整功能。
- 不将预编译实验 DSO 伪装为源码构建。唯一原核心 DSO + NAPI/GLES 父库；核心使用明确 365 个宿主导出，不能恢复 `_Z*` 通配或外露 canonical C GL/EGL 符号。
- HAP 保留 OLEX2/GL4ES 许可、完整 GLU 原归档及第三方来源说明；不据此声称正式发行合规已经认证。

## 当前标准验证基线与边界（0.22.0）

- 当前包 SHA 为 `d1ef66c779452135d803644ce62a47d672702f58fb3274d7582df98aeb830552`；目录指针 `out/app/latest-build-directory.txt`，运行指针 `reports/app/last-run.json`。重建后必须以实际包重新核对并更新汇总，不能沿用此摘要。
- 当前真实编译 529 单元，349 份上游 cpp / 442 = 78.96% 仅为**实际包构建纳入比例**；不得称功能完成度、测试覆盖率或工期。`functional_completion_percent` 始终为 null，完整 P1～P8 仍未完成。
- 主画面仍使用已有 GLES3 适配。新增原绘图按钮仅验证三种合成诊断图元经真实 TGlRenderer/GLU/GL4ES 往返当前窗口，不是完整原 UI、业务或渲染替换。
- 复用既有 ApplicationRuntime/TXApp 空核心，不另建单例。135 宏/38 函数只是准确注册数；两项真实目录宏和有限 app.*/pc.* 行为不代表完整命令库可用。
- 原有 187 项界面检查逐项按原顺序保留，新增 6 原绘图汇总 + 1 严格布局，共 194。6 轮各 126 独立检查、4 个不同 PID、最后同进程 run 1/2/3；不可仅修改计数或删弱旧检查。
- 图形切换顺序必须是解除临时绑定 → 销毁临时 context → 最后重新绑定主 context；随后验收真实下一帧和 GPU 状态，不能只检查销毁前状态。
- 每 PID/run 独占产物目录，目标与本地的可读诊断产物 SHA 核对；不得全局 glob 混入旧证据。比较宿主前等待既有延迟保存 `saveStatus=saved`，不排除 workspaceState 相等检查。
- 控件要求 `bounds == origBounds` 并在面板内，不能用裁剪后 bounds 冒充完整可见。输入层是可聚焦透明 Button，SURFACE 只显示像素；滚轮注入同次移动/点击/滚动并验证真实 axis 计数与距离变化。
- 保留未改 `upstream/sdl/esphere.h` 与共享提交分支的精确等价；圆盘退化闭合、边缘尾项、源线框边界和拒绝范围不能被普通扇形或新增对角线替代。
- 本机 18 CTest、原有目标端 17 组及新增灯光 CPU 检查、194 界面、329 汇总一致性各自独立。模型与合成拒绝夹具不是目标运行观察。
- 仍保留独立 GL127/EGL3 失败、初始 capability shader 失败及限定 FPE 适配边界。直接读取已安装受保护 HAP/DSO 被拒，不改权限或身份，不能声称目标代码文件 SHA 已直接核对。
- 当前是未签名 debug 模拟器验证包；真机、物理键鼠、正式签名和发行资格未验证。所有旧 `originalOlex2RendererTested=false` 完整资格标记与新“有限原窗口路线已验证”分开，不据后者扩大前者。

## 入口与后续

- 构建：`./scripts/build-baseline.sh host|ohos`、`./scripts/build-app.sh`。
- 运行：`scripts/run-device-smoke.py`、`scripts/run-light-defaults-smoke.py`、`scripts/run-app-smoke.py`，均显式 `--target`。
- 检查：`scripts/check-app-package.py`、`scripts/check-adaptation-status.py`、`python3 -m unittest discover -s scripts/tests`。Node 模型用 `--test --test-reporter=tap`，不放宽解析器迎合不同日志前缀。
- 现役说明：`README.md`、`docs/原版绘图链路正式源码接入-2026-11-09.md`、`docs/完整适配路线-2026-11-09.md`。历史细则与旧摘要保存在 `reports/app/0.21.0-before-0.22-summaries/AGENTS.md` 对应归档文件和各版本文档中，不作为当前完成声明。

## 0.22.11 完整诊断回归候选（历史资格，不能沿用到新包）

- 标准 0.22.0 / 16补丁 / 365导出及 `out/app` 指针不变。上一份完成200项界面回归的候选为 **0.22.11**，`out/gx-application-evaluation/source11/`；HAP `out/gx-application-evaluation/app-builds/20261109-223910-5a1c0e/entry-default-unsigned.hap`，SHA `0107a0f6bf84d975bfd6d1ddc105922a78c73e5a9ea892c5256f46b2445c82dc`。19补丁、601实际单元、379上游cpp、367明确核心导出仅为构建事实。现役结论只认 `reports/gx-application-platform-status.json`，功能比例始终null。
- 已验证的source9和source11包源码/捕获配方保持冻结，后续生成新镜像。新0019补丁修正一维生成/绑定/上传、按目标类型恢复及重建后ID索引；固定GL4ES的一维读取只在候选宏下经公开二维别名适配，作用域恢复原二维绑定，**不改vendor、不泛化到其他GL实现**。
- 真实当前窗口一维验证是正常 width3/5 × RGB/RGBA 四种色条，每种两代资源；六轮各64独立检查全部通过。重建在**同一个context**中，不是跨context迁移。CPU读回不是GPU证明，必须有目标SHA绑定的窗口像素和驱动状态。长期所有者仍 `PlatformApp : TXApp`，原 `TGXApp::LoadTextures` 真GPU未接通，不能并存第二应用单例。
- 本包200/200 UI通过：`out/gx-application-evaluation/source11/reports/app/20261109-225628-1ef641/result.json`。旧194项完整保留，另6个一维汇总；每轮仍保留126原窗口检查，新增64一维检查。原窗口4个不同PID、最后同进程run1/2/3；UI全部7个明确PID已严格收集：`reports/emulator/gx-texture1d-platform-app-20261109-230906-87460b18/result.json`。不能将4个绘图PID当全部UI范围，不能glob拼旧数据。
- 本包固定12启动全部通过：`reports/emulator/gx-startup-audit-20261109-231011-2c44a583/result.json`，原fd数量相等检查未放宽。先取得本包完整UI，再用 `collect-gx-texture1d-evaluation.py` 和 `--texture-hook --owned-memory-resource-proof --require-post-decode-control --png-rgb-backend` 收集全部UI PID。用 `record-gx-texture1d-progress.py`绑定证据；旧PNG记录器限0.22.9，旧平台记录器限0.22.8。
- 原187 UI检查顺序不变；候选精确三对FPE着色器（未纹理/RGB/RGBA）均成功，标准验证器仍精确要求原两份。系统选择框必须等精确导出URI可见或搜索框**实际聚焦**，不能把仅显示搜索框当可输入；保留精确文件名、URI、字节与设置恢复检查。
- PNG组件仍在 `experiments/gx-png-image/`：固定、原样构建libpng/zlib全部885原始文件；不改生产vendor。保留alpha=0隐藏RGB，不做有损反预乘/gamma变换，完整libpng处理灰度/调色板/16位/Adam7。先有界读入自有内存并关闭文件，C层setjmp/longjmp不跨C++析构。原LoadTextures保持历史x/y约定。
- 两个生成头 `pnglibconf.h`/`zconf.h` 必须绑定实际字节、Ninja依赖及固定配方。候选引擎 `scripts/build_provenance_png.py` 独立扩展；**不修改标准锁定的 `scripts/build_provenance_graphics.py`**。PNG旧独立CPU的3×624只属于其报告，不能替代新HAP自身验证；ImageKit透明失真观察保留。
- 本轮558完整Python、标准18 CTest、一维本机1 CTest、原二维2 CTest、26原窗口反例、12扩展证据反例、329标准一致性通过。一维830接口断言仍为明确模拟GL，绝不入HAP；19补丁正反精确重放143项通过。详见 `experiments/gx-texture-1d/README.md`。
- 保留0.22.10一维读取失败、0.22.11初期验证器数量/缺宿主参考/选择框未聚焦失败。历史0.22.8的12启动5/7、0.22.5同批离线重核9/3及中断UI、0.22.3的2/10、旧工具误报仍保留；新成功不证明旧fd差异已经归因。不删原数量检查、不随机重试到绿。
- 已知自有文件/缓冲/公开对象释放与libpng分配回调分别记账，不宣称全进程或SDK内部无泄漏；部分fd仍fstat errno13，完整元数据扫描保持未通过。不改权限/身份，不强关未知fd，受保护安装代码不能直接读SHA的边界不变。
- 原MateBook Pro保持启动，仅 `127.0.0.1:5555`，ARM64/2in1/emulator/API24；未代接受协议、reset、清数据、换实例或改SDK/系统设置，不操作其他包。
- 仍缺原LoadTextures真GPU上传/替换/过滤/恢复、一维实际跨context迁移与未验收mipmap/采样格式、字体、持久原scene/context/renderer和唯一长期所有者、完整UI/资源/Python/插件及P1～P8。每轮close_gl4es后不能复用旧持久资源；原解释器锁非递归，桥内已持锁不能再调用会持同一锁的方法。不运行 `out/gx-application-evaluation/unexecuted-passed-*-template.py.txt`。


## 上一份长期 GX 候选（0.22.13；历史资格）

- 标准0.22.0不变；source9/source11/source12/source13及其捕获配方保持冻结。当前开发候选为source13 / 0.22.13，HAP `out/gx-application-evaluation/app-builds/20261109-233724-dede29/entry-default-unsigned.hap`，SHA `c4697aa9334cdb2004dc06b5a6b55634a6746310a55a5f9fcf82ef28f38591a8`。602实际单元、379上游cpp、19补丁、367明确导出仅为构建事实。
- `PlatformApp : PngTextureGXApp : TGXApp` 成为唯一长期所有者；真实原renderer和平台scene跨窗口租约保留。长期所有者不再是TXApp，但默认主画面仍为GLES3适配。原LoadTextures尚未接入真实GPU；字体接口明确失败；仍不得并存第二应用单例。
- 模拟器 `reports/emulator/persistent-gx-window-20261109-233923-7ce43126/result.json` 六轮通过：四个独立PID，最后同PID的run1/2/3；每轮126原窗口+10长期生命周期+64一维检查。原像素/矩阵/驱动/宿主恢复门禁保留，场景销毁期望明确从临时1改为长期0。实际三地址同进程不变，租约平衡，清理在close_gl4es之前。
- **0.22.13已实测206/206诊断UI、全部7个UI PID平台资源收集、固定12/12启动通过。**原200项名称/顺序完整保留，新增6生命周期汇总；六轮仍各126原窗口+10生命周期+64一维。12次均额外验证当前PID日志/快照、新包身份与真实长期owner初始化，共120项；不以早期CPU文件冒充完整启动。根现役报告已更新；旧0.22.11与本轮前partial13分别保留为 `reports/gx-application-platform-before-persistent-runtime-status.json` 和 `reports/gx-persistent-runtime-before-ui-status.json`。这不是完整原UI或完整适配，整体in_progress/false，比例null。
- 新入口为 `prepare-persistent-gx-candidate.py`、`run-persistent-gx-window-smoke.py`；完整诊断记录由 `record-persistent-gx-ui-progress.py` 写入。真实GX宿主382编译单元/379上游cpp、276断言，无GL context或数据加载；顶层193/46，递归目录含子库196/52。参考由真实原processor的LstMac/LstFun日志和注册对象逐字节对照生成，见 `reports/persistent-gx-host-catalog-reference.json`，不可混用顶层与递归数量。冻结source13不改；独立 `persistent-gx-ui-tools13-v2` 的64文件配方已精确重放，来源验证仍指向真正source13。
- runtime窗口方法锁序为NAPI所有者 → Impl mutex → OriginalInterpreterGuard。C ABI不得先持原解释器锁再调用它；桥内不得递归调用会加同一锁的方法。场景复制ABI回调值，解绑后清空；逐图元异常清理先于对象销毁，表面清理/解绑先于close_gl4es。
- 完整606 Python、标准18 host CTest、真实GX元数据host 1 CTest/276断言、另1 modeled场景CTest/83断言、48隔离验证器测试通过；原10文件候选生成与新64文件UI工具重放分别精确。host模型未入HAP且不是原scene/GL资格；真实元数据host也不是GPU资格。全部旧失败保留；新捕获要求精确包身份+当前PID日志完整报告。当前报告28个直接绑定、HAP及锁定标准来源引擎已另行核对。
- 下一步原LoadTextures真GPU和真实日志增量、实际持久纹理跨context迁移、字体及完整P1～P8。现有一维manager仍临时、资源重建仍同context；19补丁不代表完整迁移资格。原LoadTextures日志不能回滚/排除，新增行为必须独立验收且保留206流程。原生改动生成新候选，不改冻结source13。详见 `docs/长期GX诊断界面与启动回归-2026-11-10.md`。


## 当前进行中的跨 context 纹理候选（0.22.14；优先于上节的当前开发包描述）

- source13、0.22.13 HAP、其206/206/7PID/12启动资格及原证据冻结。当前安装开发包为source14b/0.22.14，HAP `out/gx-application-evaluation/app-builds/20261110-002927-db80f4/entry-default-unsigned.hap`，SHA `6e7c3c83aa54a226b5fc978ab916e27cf98643b52bb8337bd641aa7e7ee52129`。603实际编译单元、379上游cpp、20补丁、367明确导出、3库来源通过；这些不是功能比例。
- 真正renderer的原TTextureManager长期持有四个width3/5×RGB/RGBA一维纹理对象。原BeforeContextChange在关闭前读取/释放，下一真实context原AfterContextChange重建；manager/四对象地址最后同PID run1/2/3不变。两个实际context衔接的CPU与整幅GPU颜色/alpha字节一致。旧context真实解除/销毁，每轮非共享新context；数字ID跨context可重复，不能要求数字必不同。
- `reports/emulator/texture-context-window-20261110-003125-3c60c303/result.json` 六轮通过：4独立PID，最后15863的run1/2/3。原126+10+64逐轮保留；新四初始各48、两连续各51，共294。初始单轮不算迁移资格。wrap_s仍每次显式设置，只证明nearest min/mag重新应用，不扩大为全部sampler迁移。
- 新0020只让原manager析构跳过已退役的~GLuint(0)，避免关闭GL4ES后再dispatch GL；活纹理正常析构不变。独立本机1 CTest/843模拟GL断言保留原830；24隔离合成验证器用例、完整630 Python、20补丁116项精确正反重放、8文件候选重放、329标准一致性通过。模拟GL不入HAP且非GPU证据。
- 根现役报告与 `reports/gx-texture-context-status.json` 已写0.22.14自身完整212诊断UI、全部7个UI PID和固定12/12启动资格；不是完整原UI或完整适配。旧0.22.13完整诊断归档 `reports/gx-application-platform-before-texture-context-status.json`，14首次局部资格归档 `reports/gx-texture-context-before-ui-status.json`。独立 `texture-context-ui-tools14` 65文件精确重放、原206项名称/顺序保留；新6迁移汇总共212项。
- `prepare-texture-context-candidate.py` 与 `run-texture-context-window-smoke.py` / `record-texture-context-progress.py` 是新入口；新原生输入 `texture-context-inputs.json`。未执行source14准备物保留，实际构建/验证仅source14b；不修改冻结14b，后续原生再生成新镜像。
- 下一步原LoadTextures真GPU/真实日志、2D实际跨context与异常恢复、字体/完整原UI/资源/Python/插件及P1～P8。功能比例null；用户要求的“完整适配完成后”跨对话消息未触发，不提前发送。最新诊断资格见 `docs/持久纹理完整诊断与启动回归-2026-11-10.md`；实验README保留首次局部验证历史，勿修改已被旧证据绑定的文件。

- 补充：UI工具65文件已精确重放，原206项完整顺序也由新212独立验证器强制保留；新24迁移+9顺序+11启动共44隔离验证器通过，新增启动夹具来自真实14当前PID快照（SHA `cb3df8d0bda067aac25f55362a4ee6ff5c917b8261533f1d4ca9c9ab84b26086`），完整650 Python及标准18 CTest通过。日志 `all-python-tests-texture-context14-ui-final.log` / `texture-context-all-verifier-tests-final.log` / `production-host-ctest-texture-context-ui.log`。
- 原212完整UI进程10618 / exec session48079已正常结束，退出0；212项全通过，未重启或补选样本。UI结果 `out/gx-application-evaluation/texture-context-ui-tools14/reports/app/20261110-003827-0b98d9/result.json`。全部7个PID收集 `reports/emulator/gx-texture-context-platform-app-20261110-005539-558b1529/result.json`；固定12次启动 `reports/emulator/texture-context-startup-audit-20261110-005649-1fa490d8/result.json`，12/12通过、120项长期owner初始化。记录器重新核对原始证据并成功写入现役三份报告，独立结果 `reports/emulator/texture-context-ui-revalidation-20261110-005745-46967776/result.json`。本轮UI实际4绘图PID/6轮，最后11685的run1/2/3，而非沿用首次窗口15863的证据。最后启动PID17082仅为本次观察，不当永久活进程。部分fd元数据errno13及完整扫描未通过仍保留。


## 首次原纹理加载候选（0.22.15；优先于上节当前开发包描述）

- 实际构建/安装/有限窗口通过为冻结source15 / 0.22.15，HAP `out/gx-application-evaluation/app-builds/20261110-010556-9b9e17/entry-default-unsigned.hap`，SHA `4660ea2aab720bbab5ea87de01c8112fe83e8629e6aaa8f7f2361e30d9a948c0`。604实际编译单元、379上游cpp、20补丁、367明确核心导出、3库来源通过；不是功能比例。source14b/15及真实捕获配方保持冻结。
- 新可见纹理按钮通过唯一长期GX owner/scene租约真正调用原LoadTextures。只用两代合成PNG，2×2/4×4再对调尺寸；原Add/Replace2DTexture和历史x/y转置、对象身份、原sampler和原日志保留，不清空/暂停/回滚日志迎合旧门禁。独立入口不删除或弱化原212流程。
- 实际六轮 `reports/emulator/texture-load-window-20261110-010844-b643d7d3/result.json` 通过：4实际PID（23296/23341/23733/24096），最后24096的run1/2/3；前四轮111、两连续轮143，共730独立原始证据检查，另24 UI快照/日志检查。原管理器及两对象跨代/两次真实2D context切换保持，CPU字节和整幅恢复GPU画面一致；独立双线性+repeat像素通过。只证明正常level0/border0/RGB，minification仅参数，不扩大到其他格式/mipmap/异常恢复。每次加载后只检查最后一个PNG decoder分配回调平衡，非SDK/全进程资源无泄漏。
- 原始证据已再读并写入现役application/persistent-runtime/texture-load三份报告，独立结果 `reports/emulator/texture-load-revalidation-20261110-011501-3f27a213/result.json`；14完整诊断归档 `reports/gx-application-platform-before-texture-load-status.json`。`reports/gx-texture-context-status.json`继续保留14资格，不能冒充15通过。所有顶层保持in_progress/false/null。
- 完整679 Python、29隔离合成验证器、16文件候选和65文件UI工具精确重放、标准18 CTest通过。日志 `all-python-tests-texture-load15-attempt1.log`、`texture-load-verifier-tests-attempt1.log`、`texture-load15-generator-replay.log`、`texture-load-ui-tools15-replay.log`、`production-host-ctest-texture-load15.log`。新验证器测试不是GPU证据。
- 历史启动记录（已终态失败，现已不运行）：exec session **41839**，实际进程 **87036**；日志 `out/gx-application-evaluation/texture-load15-full-ui-attempt1.log`，目录 `out/gx-application-evaluation/texture-load-ui-tools15/reports/app/20261110-011921-eefb60`。先核实同一handle/真实进程终态，不因超时重启，不并行操作模拟器。`prepare-texture-load-ui-tools.py`只按15身份/输入清单重用原65文件配方，`texture_load_application_validation.py`仅把原212分析器的版本护栏映射到15；原14脚本字节不改。
- 15自身完整212/全部UI PID平台资源/固定12启动尚未完成，不能沿用14结果。下一步先等当前运行并独立分析；后续收集/启动仍需明确15版本profile，不能拿14脚本默认护栏直接改成宽泛接受。
- 源码复核发现15两入口共用run，而原1D探针以run>1判断持久纹理存在，可能影响首次新纹理→首次原绘图的交替使用。仅源码发现，**未**在当前长UI里插入测试或声称目标已重现。已另备source16 / 0.22.16，仅3文件变化（入口独立计数头、共享包装、版本），未构建/安装/目标运行。`experiments/gx-texture-load/CMakeLists.txt`只为该计数提供独立host测试，无GL/设备执行，未入HAP。后续优先构建16并验证交替入口，不让单一路径掩盖切换缺口。
- 后续还需原纹理错误路径、其他格式/异常恢复、字体、完整原UI/菜单/命令/文档/资源/Python/插件和P1～P8。功能比例null；真机/物理输入/签名发行未验证，条件跨对话消息未发送。详见 `docs/原纹理加载真实窗口接入-2026-11-10.md`。

补充：准备中16的独立host计数为1项CTest / 62断言通过，明确无GL/设备执行且未入HAP。源码复核与准备边界见 `reports/gx-texture-load-route-status.json`，不据此声称16已构建或交替入口已在目标通过。


## 当前权威开发状态：source16 / 0.22.16（覆盖上节15/准备16的旧状态）

- source16已真实构建、安装并通过混合入口验证；HAP `out/gx-application-evaluation/app-builds/20261110-012727-548614/entry-default-unsigned.hap`，SHA `046184a01358e6c22f3f6ee0a818475b790c6d71a71cdee2b022cf01f9c2e83a`。604实际编译单元、379上游cpp、20补丁、367明确导出、3库来源通过。source14b/15/16及构建捕获全部冻结，不修改已验证原生输入。
- 16只修正三文件的入口独立run计数；真实租约仍是全部动作累计。host计数1CTest/62断言通过且未入HAP、无GL/设备运行。
- 真实混合 `reports/emulator/texture-load-mixed-20261110-013538-184d0455/result.json`：PID16793和16825两种冷启动顺序，每PID6动作，12/12通过；每入口run1/2/3，全动作真实租约1～6。756原窗口+60生命周期+384临时一维+300长期一维+788加载+114混合检查，24真加载通知；两种纹理各4次按入口连续恢复比较不冒充全部物理切换数。原126门禁与按钮完整可见条件保留。
- 独立复核 `reports/emulator/texture-load-mixed-revalidation-20261110-014629-d6ed668c/result.json` 已写入现役4报告，仍in_progress/false/null。texture-context报告仍是14资格。初次记录器整数/JSON字符串键比较失败保留，严格canonical修复后才通过，不修改原始GPU证据或删检查。
- 15完整UI已终态退出1、PID87036消失；81已记录项通过不等于212通过。desktop_enter_application_interpreter后dumpLayout等40秒超时，根因未归因，不能声称16计数修复解决该超时。15失败报告及两份归档均保留，详见纹理加载文档。
- **16完整212 UI已终态退出0：exec session31670 / 原host PID30495已消失**；日志 `out/gx-application-evaluation/texture-load16-full-ui-attempt1.log`，目录 `out/gx-application-evaluation/texture-load-route-ui-tools16/reports/app/20261110-014744-93cad4`。212/212通过，未重跑该UI；后续串行收集和启动也已结束，具体以本节末尾最新结论为准。
- 706完整Python、标准18CTest、计数1CTest/62断言、16的65文件UI工具重放通过。新16专用平台/启动profile14项隔离测试通过，尚未设备执行。入口 `collect-gx-texture-load-route-evaluation.py` / `run-texture-load-route-startup-audit.py` 只接受16；保留全部UI PID、四个资源/PNG/控制开关、固定12启动和10真实owner门禁。执行源码及关键依赖原字节/SHA必须归档；15工具不放宽版本guard。
- 下一步先取得当前UI终态并独立分析。成功后收集16自身全部UI PID，再做固定12启动并独立核验记录；失败则保留真实失败调查，不增超时/删门禁凑绿。字体、完整原UI/资源/命令/文档/Python/插件、P1～P8及真机/物理输入/签名发行仍未完成；跨对话消息条件未满足，未发送。


## 最新完成的16诊断资格与下一步（2026-11-10 02:08之后，覆盖前面的待回归状态）

- 16自身212/212 UI、全部7个UI PID资源收集、固定12/12真实启动已实测通过并独立复核。UI目录仍 `texture-load-route-ui-tools16/reports/app/20261110-014744-93cad4`，不是新一轮UI。六轮294长期纹理检查及两个实际context衔接保留；12次逐次核对真实当前PID日志和10项长期owner门禁，共120项。原fd相等检查和4个资源/PNG/控制开关不变。
- 全UI PID为967/1219/8165/11416/17418/26602/27674。平台报告 `reports/emulator/gx-texture-load-route-platform-app-20261110-020453-cc7f9aa5/result.json`，启动报告 `reports/emulator/texture-load-route-startup-audit-20261110-020502-b6bbd76e/result.json`；最后目标PID22138。对应16专用profile的执行源码和5关键依赖原字节/SHA保留。
- 原UI session31670/PID30495、串行driver session82460/PID59602均已终态退出0，当前没有应继续等待的本轮长测试。driver源码与结果 `out/gx-application-evaluation/texture-load16-completion-driver-attempt1.{py,json}`；不要重新跑同一UI或随机启动，失败/成功链路全部保留。
- 完整720 Python在162.143秒通过（`all-python-tests-texture-load16-ui-final.log`），14精确profile隔离用例、标准329一致性、三文件候选/清单精确重放和65文件UI工具重放通过；原host计数1CTest/62断言与未改标准18CTest证据保留。
- 最终独立结果 `reports/emulator/texture-load-route-ui-revalidation-20261110-020829-343f837f/result.json`；四份现役报告同为16的混合/212/7PID/12启动有限资格，179项绑定大小/SHA最终核对通过。混合资格归档 `reports/gx-texture-load-route-before-ui-status.json`；texture-context报告仍保留14资格，15真实失败继续保留且根因未归因。
- 下一步真实字体接入已开始：`experiments/gx-font/native/harmony_glyph_raster.*`使用真实NativeDrawing测量/基线/advance/位图，ARM64静态库和链接检查程序编译链接通过，32公开接口导入。**未运行、未集成HAP、未操作模拟器，字体仍未实现**。首次SDK驱动未使用参数警告造成编译失败保留，只忽略该驱动警告，其他实现源警告仍为error。源码快照 `out/gx-font/source-snapshot-20261110-020648` 和来源报告 `reports/gx-font-raster-preparation-status.json`；不能把静态/链接成功当像素、原字体或无泄漏资格。
- 字体后续必须做256字形共同基线/单元拼接、原TGlFont CharFromRGBArray/CreateGlyphsFromRGBArray、PlatformWindowScene字体创建/缩放/context恢复、字体资源/描述/DPI/对话框；原生集成生成新候选，冻结source16与其验证HAP不改。不以新标签绘制替代原字体。仍保留完整原UI/命令/资源/文档/Python/插件、P1～P8和真机/物理输入/签名发行范围；整体in_progress/false/null，跨对话消息未发送。


## 当前权威状态（覆盖前述16当前状态）：0.22.19 原字体有限窗口资格（2026-11-10）

标准发布前基线仍为0.22.0，不能把下面的候选误写成完整软件完成。
当前开发安装包是source19 / 0.22.19，HAP位于
`out/gx-application-evaluation/app-builds/20261110-024106-759123/entry-default-unsigned.hap`，
SHA-256 `746ee8334421c2ebba937f646dc8d6b869fdb9c494dd1bc292a66732e9831f13`。
608实际C/C++编译单元、379上游cpp、20补丁和3份包内原生库来源核对通过；这些不是功能完成度。

原NativeDrawing字形、共同基线/256单元、真正TGlFont原显示列表已经串通，
在原MateBook Pro模拟器同一PID10798中完成三轮真实窗口，18→27→18字号变化和两次context连续恢复。
85项独立检查重新读取原始产物通过，9幅实际窗口与独立字形布局逐字节一致；
原整数`glRasterPos2i(32,32)`保留，位置模型使用实际构建ARM64库中的float32 FMLA，
不平移参考图、不放宽像素比较、不改变测试位置凑绿。主画面下一帧、选择/EGL恢复及旧/新控件完整可见也保留。
这只覆盖明确native字体描述与常规ASCII `Ab09`，**不等于完整字体或完整原UI可用**。

source17布局裁切门禁失败、source18整数边界参考模型失败和所有旧证据均保留；
source19仅额外记录真实矩阵，并绑定实际DSO反汇编。source16～19及其原生输入冻结。
19生成器2508文件精确重放通过；UI工具65个配方文件，连同引用目录JSON、宿主目录日志、工具清单总计68文件，
与16同口径集合相同，不是新增缓存或删除门禁。
三路测试覆盖全部六种有向切换，相同入口各run1/2/3、全动作租约1～9。
每PID最终9次字体请求严格闭合；加载路线本身没有直接字体计数遥测，不将跨边界闭合冒充该路线直接观测。

现役有限资格见`reports/gx-font-status.json`和`reports/gx-application-platform-status.json`。
此前16自己的212 UI/7PID/12启动/混合入口资格仅作归档，不能转记到19。
19自身212/212诊断UI、全部7个UI PID资源收集、固定12/12真实初始化启动以及2PID/18动作的字体/原绘图/加载交替已实测通过并独立重新分析；没有继承16的通过资格。
完整784 Python回归通过（含原743项、新15记录器、14平台/启动profile及12三路混合隔离测试）；实际日志保留，模型测试不冒充目标运行。
标准18 CTest及字形单元/三路计数2 CTest通过，宿主模型未入HAP、不代表GPU资格。

完整字体描述/资源/族与样式选择/DPI/广泛字符、完整原UI/菜单/命令/文档/资源/Python/插件、P1～P8、
真机/物理输入/正式签名发行仍未完成，`full_port_complete=false`、`functional_completion_percent=null`。
条件跨对话消息仍未发送；不reset/卸载/清数据/换模拟器、不代接受协议、不更改SDK/权限。



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


### 21本轮三路交替已终态（2026-11-10，优先于历史pending描述）

执行session17621、独立重验39972、完整885回归90843和合并记录68668均已正常终态；没有正在运行的本轮模拟器测试。
真实18动作报告`reports/emulator/vector-font-state-mixed-20261110-042347-44ff22e8/result.json`，
完整当前有限资格合并`reports/emulator/vector-font-state-revalidation-20261110-042845-8f1dfdc4/result.json`。
最终6份汇总/317份证据/两个实际最终18次请求核对见`out/gx-application-evaluation/vector-font21-mixed-final-consistency.json`。
不要无故重启本轮全诊断，也不能将阶段性通过当作适配完成；下一项实际功能是原字体资源和族/样式选择等缺口。
已查看的原字体资源入口为`upstream/gxlib/wxglscene.cpp:372`，MetaFont描述在`upstream/glib/glscene.cpp:156`及`:180`，
资源签名`ofnt`后为ZIP，entry中原uint16宽高与256字符bit-array；原8～28及72字号的缩放语义尚未移植。
这些仅是本地原代码定位，不等于已经实现资源导入；不得用默认NativeDrawing字体冒充原字体资源成功。
标准、source20/21及所有HAP保持不变；全目标in_progress/false/null；条件跨对话消息sent=false。
