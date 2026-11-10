# 原纹理加载窗口接入

当前已实际构建/安装/有限窗口验证的是source15 / 0.22.15；source16仅离线准备。
完整事实和原始证据见 `../../docs/原纹理加载真实窗口接入-2026-11-10.md`，
现役机器报告 `../../reports/gx-texture-load-status.json`。

`native/original_texture_load_probe.*`调用真正TGXApp::LoadTextures，保留真实日志和同名纹理对象，
用实际窗口验证PNG读取、尺寸替换、sampler设置和正常2D跨context恢复。
`synthetic-fixtures.json`与生成头是四张明确合成颜色PNG的输入参考，绝不是运行或GPU观察。
生成器 `../../scripts/generate-texture-load-fixtures.py --check`必须精确重放。

构建/实际运行来自冻结source15独立候选，不修改标准包或vendor。
新入口和原212项UI分别验收，不能缩减后者。
`tests/route_runs.cpp`只测试准备中16的入口独立计数，不执行GL/设备，不打入HAP。
当前新包完整诊断UI仍在运行，不能并行操作模拟器或因观察超时重启。

完整原UI、字体、资源、Python、插件及完整适配没有完成，功能比例null。
