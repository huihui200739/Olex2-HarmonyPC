# 原绘图器纹理的实际跨 context 保留（进行中）

仅合成色条，不加载结构文件或运行研究计算。完整 Olex2 仍未适配完成。

## 新候选与最小补丁

当前候选为0.22.14/221400，源码 `out/gx-application-evaluation/source14b`。
由冻结source13生成，20个上游补丁；新增0020仅使已经由BeforeContextChange退役的纹理
不在管理器析构时再次调用GL。活纹理正常析构仍逐个删除，空管理器和已detach管理器不需要context。
生产标准、source13与其原HAP保持不变。

真正 `TGXApp` 所有的原renderer/scene及其TTextureManager现在持有四个长期纹理对象。
每轮旧context中由原BeforeContextChange读取/释放，下一实际context由原AfterContextChange重建，
并用原对象、名称、manager新ID索引和保留的min/mag配置绘制。包装层不伪造第二TGX单例。
最近邻过滤被重新应用；wrap_s在每次Draw中显式设置，**不能声称本候选独立证明了wrapping属性迁移**。

## 实际模拟器观察

- HAP：`out/gx-application-evaluation/app-builds/20261110-002927-db80f4/entry-default-unsigned.hap`
- SHA256：`6e7c3c83aa54a226b5fc978ab916e27cf98643b52bb8337bd641aa7e7ee52129`
- 603实际编译单元、379上游cpp、20补丁、367明确导出、3份包内原生库来源通过。
- 实际报告：`reports/emulator/texture-context-window-20261110-003125-3c60c303/result.json`。
- 四独立PID：14582、14590、15241、15863；最后PID实际run1/2/3，两个真实context衔接通过。
- 每轮旧126原窗口+10长期生命周期+64临时一维检查保留；另四种width3/5×RGB/RGBA长期纹理。
- 初始四轮各48项长期纹理检查，连续两轮各51项，共294项；run1单轮明确不是迁移资格。
- 最后三轮同一个manager和四个原纹理对象地址保持一致；原context均实际解除并销毁，
  下一轮重新创建非共享context。新环境CPU读回、实际窗口GPU色条/alpha和整幅字节与上一轮一致，
  不以CPU相等或同context重建代替这个证明。数值ID可跨context重复，不以数值不同作为必要条件。

## 本机与验证工具

`original_texture_context_mocked` 是真实上游方法+明确模拟GL，1 CTest/843断言，
保留原830断言并加13项detached析构约束；不创建GPU context，未进入HAP。
24个隔离合成验证器用例检查旧context未销毁、对象更换、字节/驱动错误、旧hash、
缺少前一轮和跨PID等拒绝路径；合成夹具不冒充真实目标端观察。
20补丁精确正反重放116项通过；8文件候选生成器重放逐字节一致。
最初未执行的source14准备物保留，修正了重复恢复调用和前置声明后才生成source14b并构建。
首次本机配置因命令路径不存在失败，保留日志；使用现有SDK内CMake后通过，没有装全局依赖。

## 未完成

0.22.14尚未取得新包自身完整诊断界面、全部UI PID平台资源检查或固定12次启动资格；
这些最新完整诊断资格仍属于0.22.13（206/206、7 PID、12/12），不能移植成新包通过。
原LoadTextures真GPU及真实日志增量、2D实际跨context迁移、mipmap/其他格式、异常资源恢复、
字体、完整原UI/资源/命令/Python/插件与P1～P8仍需继续。
当前验证的是正常level0/border0一维RGB/RGBA；不声称全进程/SDK无泄漏、真机或发行已验证。
