import nativeBridge, { GraphicsStatus, UtilityReport, CheckResult, ApplicationRuntimeStatus, OriginalLogSnapshot, CoreCatalogReport } from 'libolex2native.so';
import { fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { OptionsDocumentTransfer, OptionsDocumentStatus, OptionsDocumentBackend, emptyOptionsDocumentStatus } from '../model/OptionsDocumentTransfer';
import { HarmonyOptionsDocumentPort } from '../model/OptionsDocumentStore';
import { KeyCode } from '@kit.InputKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { DesktopState, DesktopStateSaveQueue } from '../model/DesktopState';
import { loadDesktopState, writeDesktopState } from '../model/DesktopStateStore';
import { usesApplicationInterpreter } from '../model/CommandSubmit';

interface WorkspaceStateStatus {
  restored: boolean;
  loadStatus: string;
  saveStatus: string;
  schemaVersion: number;
}

interface DesktopCommandStatus {
  sequence: number; input: string; passed: boolean; changed: boolean; applied: boolean; dispatched: number;
  source: string; message: string; error: string;
}

interface ApplicationFunctionStatus {
  sequence: number; input: string; passed: boolean; optionsChanged: boolean; text: string; error: string; source: string;
}
interface OriginalLogAudit {
  attached: boolean; posted: number; dropped: number; truncated: number; captureFailures: number;
  clearCount: number; clearedEntries: number; entryCount: number; retainedBytes: number;
  textSha256: string; source: string; error: string; firstText: string; lastText: string;
}
interface CoreCatalogStatus {
  sequence: number; passed: boolean; exposedMacros: number; postedBefore: number; postedAfter: number;
  input: string; error: string; source: string;
}
interface InstalledPackageIdentity {
  bundleName: string; versionName: string; versionCode: number; error: string; source: string;
}

interface OriginalRendererProbeResult { limited_path_verified: boolean; }

interface RuntimeRecord {
  installedPackage: InstalledPackageIdentity;
  scope: string;
  utilityPassed: boolean;
  checkCount: number;
  checks: CheckResult[];
  graphics: GraphicsStatus;
  upstreamGeometryTested: boolean;
  upstreamSubmissionTested: boolean;
  originalOlex2RendererTested: boolean;
  fullPortComplete: boolean;
  keyEvents: number;
  axisEvents: number;
  viewportHovered: boolean;
  workspaceState: WorkspaceStateStatus;
  desktopCommand: DesktopCommandStatus;
  applicationRuntime: ApplicationRuntimeStatus;
  applicationFunction: ApplicationFunctionStatus;
  optionsDocument: OptionsDocumentStatus;
  originalLog: OriginalLogAudit;
  coreCatalog: CoreCatalogStatus;
  originalLogView: boolean;
}

@Entry
@Component
struct Index {
  @StorageLink('olex2FilesDir') private filesDir: string = '';
  @StorageLink('olex2BundleName') private bundleName: string = '';
  @StorageLink('olex2VersionName') private versionName: string = '';
  @StorageLink('olex2VersionCode') private versionCode: number = 0;
  @StorageLink('olex2PackageIdentityError') private packageIdentityError: string = '';
  @State private originalRendererProbe: string = '';
  @State private originalTextureLoadProbe: string = '';
  @State private originalFontProbe: string = '';
  @StorageLink('olex2FontMetricsReady') private fontMetricsReady: boolean = false;
  @StorageLink('olex2FontMetricsError') private fontMetricsError: string = '';
  @State private activePanel: number = 0;
  @State private commandText: string = 'pc.help';
  @State private commandMessage: string = '原版解析器已接入视图命令；完整命令库未接通';
  @State private commandError: string = '';
  @State private commandPassed: boolean = false;
  private commandStatus: DesktopCommandStatus = { sequence: 0, input: '', passed: false, changed: false, applied: false, dispatched: 0, source: '', message: '', error: '' };
  @State private applicationRuntime: ApplicationRuntimeStatus = { initialized: false, optionsRestored: false, licenseVerified: false, commandHints: true, lastSaveCommitted: false, originalCoreInitialized: false, originalCoreEmptyFile: false, originalCoreMacroCount: 0, originalCoreFunctionCount: 0, originalCoreFileCount: 0, originalFunctionCount: 0, optionsCount: 0, saveRevision: 0, baseDir: '', instanceDir: '', sharedDir: '', configDir: '', licenseSha256: '', optionsSha256: '', platform: '', error: '', source: '' };
  private applicationFunction: ApplicationFunctionStatus = { sequence: 0, input: '', passed: false, optionsChanged: false, text: '', error: '', source: '' };
  @State private originalLogView: boolean = false;
  @State private originalLog: OriginalLogSnapshot = { attached: false, posted: 0, dropped: 0, truncated: 0, captureFailures: 0, clearCount: 0, clearedEntries: 0, entryCount: 0, retainedBytes: 0, textSha256: '', source: '', error: '', entries: [] };
  private coreCatalog: CoreCatalogStatus = { sequence: 0, passed: false, exposedMacros: 0, postedBefore: 0, postedAfter: 0, input: '', error: '', source: '' };
  private optionsTransfer: OptionsDocumentTransfer = new OptionsDocumentTransfer();
  @State private optionsDocument: OptionsDocumentStatus = emptyOptionsDocumentStatus();
  @State private optionsTransferBusy: boolean = false;
  @State private checks: UtilityReport = { passed: false, checks: [], error: '' };
  @State private graphics: GraphicsStatus = {
    surfaceReady: false, initialized: false, pixelVerified: false, geometryVerified: false,
    coverageVerified: false, meshValidated: false, wireframe: false, meshGeneration: 3, meshRevision: 0,
    submissionValidated: false, submittedVertices: 0, submissionSource: '',
    originalPolygonVerified: false, originalPolygonBatches: 0, originalPolygonVertices: 0, originalPolygonNormalCommands: 0, originalPolygonColorCommands: 0, originalPolygonSource: '',
    linePathProbe: { verified: false, stateRestored: false, contextRestored: false, source: '', error: '', cases: [] },
    polygonProbeContextRestored: false, polygonProbeStateRestored: false, polygonProbeVerified: false, polygonProbeSource: '', polygonProbeInputVertices: 0, polygonProbeTriangles: 0, polygonProbeNormalCommands: 0, polygonProbeColorCommands: 0, polygonProbeSamples: [],
    basisMatrixVerified: false, matrixUniformVerified: false, basisMatrixSource: '',
    modelViewDepth: 0, projectionDepth: 0, matrixUniformMaxError: 0,
    modelViewMatrix: [], mvpMatrix: [], normalMatrix: [],
    originalLineVerified: false, originalLineBufferVerified: false, originalLineVertices: 0, originalLineColorCommands: 0, originalLineWidthQueries: 0, originalLineWidthCommands: 0, originalLineSource: '', originalLineIndexSha256: '',
    originalTriangleVerified: false, originalTriangleVertices: 0, originalTriangleNormalCommands: 0, originalTriangleColorCommands: 0, originalTriangleSource: '',
    meshKind: 'sphere', sourceBatches: 0, discardedTailVertices: 0, degenerateTrianglesSkipped: 0, geometryColorSamples: 0,
    materialPreset: 0, materialRevision: 0, materialFlags: 783, materialSource: '', materialCanonical: '', materialVerified: false, materialCommandCount: 0, materialCommandsSource: '',
    ambientR: 0, ambientG: 0, ambientB: 0, diffuseR: 0, diffuseG: 0, diffuseB: 0, emissionR: 0, emissionG: 0, emissionB: 0,
    specularR: 0, specularG: 0, specularB: 0, shininess: 0, specularComparedPixels: 0, specularChangedPixels: 0, specularMaxError: 0, specularMaxDelta: 0, specularReferenceVerified: false, specularSamples: [],
    rowMeanScale: 0, rowMinScale: 0, rowMaxScale: 0,
    colorPreset: 0, colorRevision: 0, colorPacked: 0, colorSource: '', colorR: .2, colorG: .8, colorB: .59, colorA: 1, colorVerified: false, readbackR: 0, readbackG: 0, readbackB: 0, readbackA: 0,
    meshVertices: 0, meshTriangles: 0, meshIndices: 0, meshSource: '',
    width: 0, height: 0, frames: 0, yaw: 32, pitch: -22, distance: 4.4, version: '', renderer: '', error: ''
  };
  @State private keyEvents: number = 0;
  @State private axisEvents: number = 0;
  @State private viewportHovered: boolean = false;
  @State private yaw: number = 32;
  @State private pitch: number = -22;
  @State private distance: number = 4.4;
  @State private lastRecord: string = '';
  @State private recordError: string = '';
  private timer: number = -1;
  private dragYaw: number = 0;
  private dragPitch: number = 0;
  private reportSequence: number = 0;
  private recordKey: string = '';
  @State private stateLoadStatus: string = 'pending';
  @State private stateSaveStatus: string = 'pending';
  private stateQueue: DesktopStateSaveQueue = new DesktopStateSaveQueue();
  private stateTimer: number = -1;

  aboutToAppear(): void {
    this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
    this.originalLog = nativeBridge.getOriginalLogs();
    this.restoreDesktopState();
    this.runChecks();
    this.timer = setInterval(() => this.refreshGraphics(), 800);
  }

  aboutToDisappear(): void {
    if (this.stateTimer >= 0) clearTimeout(this.stateTimer);
    this.stateTimer = -1;
    this.flushDesktopState();
    if (this.timer >= 0) clearInterval(this.timer);
    this.timer = -1;
  }

  private submitCommand(): void {
    if (usesApplicationInterpreter(this.commandText)) this.executeApplicationFunction();
    else this.executeDesktopCommand();
  }

  private executeApplicationFunction(): void {
    const sequence: number = this.applicationFunction.sequence + 1;
    try {
      const result = nativeBridge.evaluateApplicationFunction(this.commandText);
      this.applicationRuntime = result.runtime;
      this.applicationFunction = { sequence: sequence, input: this.commandText, passed: result.passed,
        optionsChanged: result.optionsChanged, text: result.text, error: result.error, source: result.source };
      this.commandError = result.error;
      this.commandMessage = result.passed ? (result.text.length > 0 ? result.text : '原版应用函数已执行') : '原版应用函数未通过';
    } catch (error) {
      this.commandError = '应用函数调用失败';
      this.applicationFunction = { sequence: sequence, input: this.commandText, passed: false,
        optionsChanged: false, text: '', error: this.commandError, source: '' };
    }
    this.graphics = nativeBridge.getGraphicsStatus();
    this.originalLog = nativeBridge.getOriginalLogs();
    this.saveRecord();
  }

  private executeDesktopCommand(): void {
    const input: string = this.commandText;
    const sequence: number = this.commandStatus.sequence + 1;
    try {
      const result = nativeBridge.executeDesktopCommand(input);
      this.graphics = result.graphics;
      this.yaw = this.graphics.yaw; this.pitch = this.graphics.pitch; this.distance = this.graphics.distance;
      this.commandPassed = result.passed;
      this.commandMessage = result.text;
      this.commandError = result.error;
      this.commandStatus = { sequence: sequence, input: input, passed: result.passed, changed: result.changed, applied: result.applied,
        dispatched: result.dispatched, source: result.source, message: result.text, error: result.error };
    } catch (error) {
      this.commandPassed = false; this.commandMessage = ''; this.commandError = '命令输入应为1～2048字节；请检查输入后重试';
      this.commandStatus = { sequence: sequence, input: input, passed: false, changed: false, applied: false, dispatched: 0,
        source: '', message: '', error: this.commandError };
    }
    this.saveRecord();
  }

  private async transferOptionsDocument(operation: string): Promise<void> {
    if (this.optionsTransferBusy) return;
    this.optionsTransferBusy = true;
    try {
      const context: common.UIAbilityContext = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const backend: OptionsDocumentBackend = {
        snapshot: () => nativeBridge.exportApplicationOptions(),
        merge: (data: Uint8Array) => nativeBridge.importApplicationOptions(data)
      };
      this.optionsDocument = await this.optionsTransfer.run(operation, backend, new HarmonyOptionsDocumentPort(context));
    } finally {
      this.optionsTransferBusy = false;
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.graphics = nativeBridge.getGraphicsStatus();
      this.saveRecord();
    }
  }

  private runChecks(): void {
    if (this.filesDir.length === 0) return;
    this.checks = nativeBridge.runUtilityChecks(this.filesDir);
    this.recordKey = '';
    this.saveRecord();
  }

  private originalLogAudit(): OriginalLogAudit {
    const snapshot: OriginalLogSnapshot = this.originalLog;
    return { attached: snapshot.attached, posted: snapshot.posted, dropped: snapshot.dropped,
      truncated: snapshot.truncated, captureFailures: snapshot.captureFailures, clearCount: snapshot.clearCount,
      clearedEntries: snapshot.clearedEntries, entryCount: snapshot.entryCount, retainedBytes: snapshot.retainedBytes,
      textSha256: snapshot.textSha256, source: snapshot.source, error: snapshot.error,
      firstText: snapshot.entries.length > 0 ? snapshot.entries[0].text : '',
      lastText: snapshot.entries.length > 0 ? snapshot.entries[snapshot.entries.length - 1].text : '' };
  }

  private originalLogText(): string {
    let text: string = '';
    for (let i: number = 0; i < this.originalLog.entries.length; i++) text += this.originalLog.entries[i].text;
    return text;
  }

  private showCoreCatalog(command: string): void {
    const result: CoreCatalogReport = nativeBridge.runCoreCatalog(command);
    this.coreCatalog = { sequence: this.coreCatalog.sequence + 1, passed: result.passed,
      exposedMacros: result.exposedMacros, postedBefore: result.postedBefore, postedAfter: result.postedAfter,
      input: result.input, error: result.error, source: result.source };
    this.originalLog = nativeBridge.getOriginalLogs();
    this.originalLogView = true;
    this.saveRecord();
  }

  private clearOriginalLog(): void {
    this.originalLog = nativeBridge.clearOriginalLogs();
    this.saveRecord();
  }

  private refreshGraphics(): void {
    this.graphics = nativeBridge.getGraphicsStatus();
    this.saveRecord();
  }

  private saveRecord(): void {
    this.scheduleDesktopState();
    const record: RuntimeRecord = {
      installedPackage: { bundleName: this.bundleName, versionName: this.versionName, versionCode: this.versionCode,
        error: this.packageIdentityError, source: 'bundleManager.getBundleInfoForSelfSync' },
      scope: '真实基础层 + 上游球面/圆环/圆盘/边缘图元提交 / 新 GLES3 适配；非完整原版渲染器或 Olex2',
      utilityPassed: this.checks.passed, checkCount: this.checks.checks.length, checks: this.checks.checks,
      graphics: this.graphics, upstreamGeometryTested: this.graphics.meshValidated && this.graphics.meshSource.length > 0, upstreamSubmissionTested: this.graphics.submissionValidated && this.graphics.submissionSource.length > 0, originalOlex2RendererTested: false, fullPortComplete: false, keyEvents: this.keyEvents, axisEvents: this.axisEvents, viewportHovered: this.viewportHovered,
      workspaceState: { restored: this.stateLoadStatus === 'restored', loadStatus: this.stateLoadStatus,
        saveStatus: this.stateSaveStatus, schemaVersion: 4 },
      desktopCommand: this.commandStatus,
      applicationRuntime: this.applicationRuntime,
      applicationFunction: this.applicationFunction,
      originalLog: this.originalLogAudit(), coreCatalog: this.coreCatalog, originalLogView: this.originalLogView,
      optionsDocument: this.optionsDocument
    };
    const serialized: string = JSON.stringify(record);
    if (serialized === this.recordKey) return;
    this.recordKey = serialized;
    this.lastRecord = JSON.stringify(record, null, 2);
    if (serialized.length <= 2800) {
      hilog.info(0x3801, 'Olex2Native', 'OLEX2_APP_REPORT %{public}s', serialized);
    } else {
      const reportId: string = `${Date.now()}-${this.reportSequence++}`;
      const parts: number = Math.ceil(serialized.length / 900);
      for (let index: number = 0; index < parts; index++) {
        hilog.info(0x3801, 'Olex2Native', 'OLEX2_APP_FRAGMENT %{public}s %{public}d %{public}d %{public}s',
          reportId, index, parts, serialized.substring(index * 900, (index + 1) * 900));
      }
    }
    if (this.filesDir.length > 0) {
      try {
        const file = fileIo.openSync(this.filesDir + '/native-verification.json',
          fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
        try { fileIo.writeSync(file.fd, this.lastRecord); } finally { fileIo.closeSync(file); }
        this.recordError = '';
      } catch (error) {
        this.recordError = '验证记录写入失败，请查看开发日志。';
      }
    }
  }

  private restoreDesktopState(): void {
    const loaded = loadDesktopState(this.filesDir);
    this.stateLoadStatus = loaded.status;
    this.yaw = loaded.state.yaw;
    this.pitch = loaded.state.pitch;
    this.distance = loaded.state.distance;
    nativeBridge.setMaterialPreset(loaded.state.materialPreset);
    nativeBridge.setColorPreset(loaded.state.colorPreset);
    nativeBridge.setPrimitive(loaded.state.primitive);
    nativeBridge.setMesh(loaded.state.generation, loaded.state.wireframe);
    this.graphics = nativeBridge.setView(this.yaw, this.pitch, this.distance);
  }

  private scheduleDesktopState(): void {
    if (!this.graphics.initialized || this.graphics.error.length > 0) return;
    const kinds: string[] = ['sphere', 'torus', 'disks', 'rims'];
    const state: DesktopState = { schemaVersion: 4, materialPreset: this.graphics.materialPreset, colorPreset: this.graphics.colorPreset, primitive: kinds.indexOf(this.graphics.meshKind),
      generation: this.graphics.meshGeneration, wireframe: this.graphics.wireframe,
      yaw: this.graphics.yaw, pitch: this.graphics.pitch, distance: this.graphics.distance };
    const action: string = this.stateQueue.stage(state);
    if (action === 'saved' || action === 'write-error') {
      if (this.stateTimer >= 0) clearTimeout(this.stateTimer);
      this.stateTimer = -1;
      this.stateSaveStatus = action;
      return;
    }
    if (action === 'pending') return;
    this.stateSaveStatus = 'pending';
    if (this.stateTimer >= 0) clearTimeout(this.stateTimer);
    this.stateTimer = setTimeout(() => {
      this.stateTimer = -1;
      this.flushDesktopState();
      this.saveRecord();
    }, 250);
  }

  private flushDesktopState(): void {
    const pending: DesktopState | undefined = this.stateQueue.pendingState();
    if (pending === undefined) return;
    const saved: boolean = writeDesktopState(this.filesDir, pending);
    this.stateSaveStatus = saved ? 'saved' : 'write-error';
    this.stateQueue.complete(saved);
  }

  private setView(): void {
    this.graphics = nativeBridge.setView(this.yaw, this.pitch, this.distance);
    this.saveRecord();
  }

  private updateMesh(generation: number, wireframe: boolean): void {
    this.graphics = nativeBridge.setMesh(generation, wireframe);
    this.saveRecord();
  }

  private primitiveName(kind: number): string {
    const names: string[] = ['sphere', 'torus', 'disks', 'rims'];
    return names[kind];
  }
  private primitiveLabel(kind: number): string {
    const labels: string[] = ['球面', '圆环', '圆盘', '边缘'];
    return labels[kind];
  }
  private currentPrimitiveLabel(): string {
    if (this.graphics.meshKind === 'torus') return '圆环';
    if (this.graphics.meshKind === 'disks') return '圆盘';
    if (this.graphics.meshKind === 'rims') return '边缘';
    return '球面';
  }
  private materialLabel(preset: number): string {
    const labels: string[] = ['经典明暗', '柔和明暗', '纯色', '高光'];
    return labels[preset];
  }
  private selectMaterial(preset: number): void {
    this.graphics = nativeBridge.setMaterialPreset(preset);
    this.saveRecord();
  }
  private selectColor(preset: number): void {
    this.graphics = nativeBridge.setColorPreset(preset);
    this.saveRecord();
  }
  private colorLabel(preset: number): string {
    const labels: string[] = ['翠绿', '琥珀', '天蓝'];
    return labels[preset];
  }
  private colorSwatch(preset: number): string {
    const colors: string[] = ['#33CC96', '#E56129', '#3885F2'];
    return colors[preset];
  }
  private selectPrimitive(kind: number): void {
    this.graphics = nativeBridge.setPrimitive(kind);
    this.saveRecord();
  }
  private viewportKey(event: KeyEvent): boolean {
    if (event.type !== KeyType.Down) return false;
    if (event.getModifierKeyState &&
      (event.getModifierKeyState(['ctrl']) || event.getModifierKeyState(['alt']))) return false;
    const code = event.keyCode;
    if (code >= KeyCode.KEYCODE_1 && code <= KeyCode.KEYCODE_4) {
      this.keyEvents += 1;
      this.selectPrimitive(code - KeyCode.KEYCODE_1);
      return true;
    }
    if (code === KeyCode.KEYCODE_W) {
      this.keyEvents += 1;
      this.updateMesh(this.graphics.meshGeneration, !this.graphics.wireframe);
      return true;
    }
    if (code === KeyCode.KEYCODE_R || code === KeyCode.KEYCODE_MOVE_HOME) {
      this.keyEvents += 1; this.resetView(); return true;
    }
    if (code === KeyCode.KEYCODE_DPAD_LEFT) this.yaw -= 10;
    else if (code === KeyCode.KEYCODE_DPAD_RIGHT) this.yaw += 10;
    else if (code === KeyCode.KEYCODE_DPAD_UP) this.pitch = Math.max(-85, this.pitch - 10);
    else if (code === KeyCode.KEYCODE_DPAD_DOWN) this.pitch = Math.min(85, this.pitch + 10);
    else if (code === KeyCode.KEYCODE_PLUS || code === KeyCode.KEYCODE_EQUALS) this.distance = Math.max(2.2, this.distance - 0.2);
    else if (code === KeyCode.KEYCODE_MINUS) this.distance = Math.min(10, this.distance + 0.2);
    else return false;
    this.keyEvents += 1; this.setView(); return true;
  }
  private viewportAxis(event: AxisEvent): void {
    const delta = event.getVerticalAxisValue();
    if (!Number.isFinite(delta) || delta === 0) return;
    this.distance = Math.max(2.2, Math.min(10, this.distance + Math.max(-0.5, Math.min(0.5, delta * 0.006))));
    this.axisEvents += 1; this.setView();
  }

  private rotateView(delta: number): void { this.yaw += delta; this.setView(); }
  private resetView(): void { this.yaw = 32; this.pitch = -22; this.distance = 4.4; this.setView(); }
  private graphicsPassed(): boolean {
    return this.graphics.surfaceReady && this.graphics.initialized && this.graphics.meshValidated && this.graphics.submissionValidated && this.graphics.coverageVerified && this.graphics.pixelVerified && this.graphics.geometryVerified && this.graphics.frames > 0 && this.graphics.error.length === 0;
  }
  private checkLabel(index: number): string {
    const labels: string[] = ['字符串替换', 'ASCII 文本转换', '双字节 Unicode 转换', '中文文本转换',
      '四字节 Unicode 转换', '字符串容器', '中文路径写入', '中文路径查找', '中文路径读取', '测试文件清理'];
    return index < labels.length ? labels[index] : '基础代码检查';
  }

  @Builder
  private NavItem(index: number, symbol: string, label: string) {
    Row({ space: 12 }) {
      Text(symbol).fontSize(19).fontColor(this.activePanel === index ? '#A6E9C7' : '#839A90').width(22)
      Text(label).fontSize(15).fontWeight(this.activePanel === index ? FontWeight.Medium : FontWeight.Normal)
        .fontColor(this.activePanel === index ? '#FFFFFF' : '#B4C3BC')
    }
    .id(`olex2-nav-${index}`).width('100%').height(48).padding({ left: 16, right: 12 }).borderRadius(10)
    .backgroundColor(this.activePanel === index ? '#2B463B' : Color.Transparent)
    .onClick(() => { this.originalLogView = false; this.activePanel = index; })
  }

  @Builder
  private CommandEntry() {
    Column({ space: 6 }) {
      Text('原版解析 · pc.* / app.*').fontSize(10).fontColor('#A8C2B5').width('100%')
      Row({ space: 4 }) {
        TextInput({ text: this.commandText, placeholder: '英文 pc.mesh torus' }).id('olex2-command-input')
          .layoutWeight(1).height(32).fontSize(11).maxLength(2048).backgroundColor('#E7EEE9')
          .type(InputType.Normal).enableAutoFill(false).enterKeyType(EnterKeyType.Go)
          .onChange((value: string) => { this.commandText = value; })
          .onSubmit(() => { this.submitCommand(); })
        Button('×').id('olex2-command-clear').padding(0).width(24).height(28).fontSize(13).backgroundColor('#355345')
          .onClick(() => { this.commandText = ''; })
      }.width('100%')
      Row({ space: 6 }) {
        Button('执行视图').id('olex2-command-run').layoutWeight(1).padding({ left: 4, right: 4 }).height(30).fontSize(11).backgroundColor('#42735A')
          .onClick(() => { this.executeDesktopCommand(); })
        Button('应用函数').id('olex2-application-function-run').layoutWeight(1).padding({ left: 4, right: 4 }).height(30).fontSize(11).backgroundColor('#42735A')
          .onClick(() => { this.executeApplicationFunction(); })
      }.width('100%')
      Button('命令帮助').id('olex2-command-help').width('100%').height(28).fontSize(10).backgroundColor('#355345')
        .onClick(() => { this.commandText = 'pc.help'; this.executeDesktopCommand(); })
      if (this.applicationRuntime.commandHints) {
        Text('pc.mesh torus / app.ConfigDir()').id('olex2-command-hint').fontSize(8).fontColor('#A2BDAD').width('100%')
      }
      Text(this.commandError.length > 0 ? this.commandError : this.commandMessage).id('olex2-command-status')
        .fontSize(9).fontColor(this.commandError.length > 0 ? '#F1B9A9' : '#A2BDAD').lineHeight(13)
        .maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis }).width('100%')
    }.id('olex2-command-entry').padding(10).margin({ left: 12, right: 12, top: 18 }).backgroundColor('#223C2F').borderRadius(8)
  }

  @Builder
  private OptionsDocuments() {
    Column({ space: 8 }) {
      Text('原格式设置文件').fontSize(14).fontWeight(FontWeight.Medium).fontColor('#253D30')
      Row({ space: 8 }) {
        Button('导出设置').id('olex2-options-export').height(30).fontSize(11)
          .enabled(!this.optionsTransferBusy && this.applicationRuntime.initialized)
          .onClick(() => { this.transferOptionsDocument('export'); })
        Button('导入设置').id('olex2-options-import').height(30).fontSize(11)
          .enabled(!this.optionsTransferBusy && this.applicationRuntime.initialized)
          .onClick(() => { this.transferOptionsDocument('import'); })
      }
      Text(this.optionsTransferBusy ? '等待选择文件…' : this.optionsDocument.state === 'passed' ?
        (this.optionsDocument.operation === 'export' ? '设置已导出，文件字节已核对' : `已合并 ${this.optionsDocument.importedOptions} 个设置`) :
        this.optionsDocument.state === 'cancelled' ? '已取消，未修改设置' :
        this.optionsDocument.state === 'failed' ? `${this.optionsDocument.error}${this.optionsDocument.destinationMayBePartial ? '；外部目标可能留有不完整文件' : ''}` :
        '通过系统选择文件；只支持原格式设置，不是完整文档打开保存')
        .id('olex2-options-document-status').fontSize(11).fontColor('#738078').maxLines(3)
    }.width('100%').alignItems(HorizontalAlign.Start).padding(14).backgroundColor('#FFFFFF').borderRadius(12)
  }

  @Builder
  private SummaryCards() {
    Row({ space: 14 }) {
      Column({ space: 7 }) {
        Text('真实 C++ 基础代码').fontSize(12).fontColor('#738078').width('100%')
        Text(this.checks.passed ? `${this.checks.checks.length} / ${this.checks.checks.length} 通过` : '等待验证')
          .fontSize(23).fontWeight(FontWeight.Medium).fontColor(this.checks.passed ? '#24674A' : '#37443D').width('100%')
        Text('直接调用上游 OlexSys 代码').fontSize(11).fontColor('#8A938E').width('100%')
      }.padding(18).alignItems(HorizontalAlign.Start).layoutWeight(1)
        .backgroundColor('#FFFFFF').borderRadius(14).border({ width: 1, color: '#E3E9E4' })
      Column({ space: 7 }) {
        Text('原生窗口绘图').fontSize(12).fontColor('#738078').width('100%')
        Text(this.graphicsPassed() ? '像素读回通过' : this.graphics.error.length > 0 ? '绘图未通过' : this.activePanel === 0 ? '等待验证' : '绘图已暂停')
          .id('olex2-graphics-readiness')
          .fontSize(23).fontWeight(FontWeight.Medium).fontColor(this.graphicsPassed() ? '#24674A' : '#37443D').width('100%')
        Text(this.graphics.version.length > 0 ? this.graphics.version : '结果由实际 EGL / GLES 状态返回')
          .fontSize(11).fontColor('#8A938E').width('100%').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
      }.padding(18).alignItems(HorizontalAlign.Start).layoutWeight(1)
        .backgroundColor('#FFFFFF').borderRadius(14).border({ width: 1, color: '#E3E9E4' })
      Column({ space: 7 }) {
        Text('移植完成度').fontSize(12).fontColor('#738078').width('100%')
        Text('基础验证阶段').fontSize(23).fontWeight(FontWeight.Medium).fontColor('#37443D').width('100%')
        Text('未集成原版 UI、Python 与插件').fontSize(11).fontColor('#8A938E').width('100%')
      }.padding(18).alignItems(HorizontalAlign.Start).layoutWeight(1)
        .backgroundColor('#FFFFFF').borderRadius(14).border({ width: 1, color: '#E3E9E4' })
    }.width('100%')
  }

  @Builder
  private CheckRows() {
    Column({ space: 0 }) {
      ForEach(this.checks.checks, (check: CheckResult, index: number) => {
        Row({ space: 10 }) {
          Text(check.passed ? '✓' : '×').fontColor(check.passed ? '#28724D' : '#B7473C').fontSize(15).width(18)
          Text(this.checkLabel(index)).fontSize(12).fontColor('#46564D').layoutWeight(1)
          Text(check.passed ? '通过' : '未通过').fontSize(11).fontColor(check.passed ? '#638675' : '#B7473C')
        }.height(32).width('100%')
      }, (_check: CheckResult, index: number) => `${index}`)
      if (this.checks.checks.length === 0) {
        Text('等待 C++ 基础代码检查…').fontSize(13).fontColor('#78877E').margin({ top: 18, bottom: 18 })
      }
      if (this.checks.error.length > 0) {
        Text(this.checks.error).fontSize(12).fontColor('#B7473C').margin({ top: 8 })
      }
    }.width('100%')
  }

  private originalProbeLabel(): string {
    if (this.originalRendererProbe.length === 0) return '测试原版绘图';
    try {
      const result = JSON.parse(this.originalRendererProbe) as OriginalRendererProbeResult;
      return result.limited_path_verified ? '原版绘图通过' : '原版绘图未通过';
    } catch (_) {
      return '绘图结果读取失败';
    }
  }

  @Builder
  private Viewport() {
    Column({ space: 0 }) {
      Row() {
        Column({ space: 4 }) {
          Text('上游几何适配视口').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).fontSize(16).fontWeight(FontWeight.Medium).fontColor('#253D30')
          Text('1–4 切换图元 · W 线框 · R 重置 · 方向键 / 滚轮').maxLines(2).textOverflow({ overflow: TextOverflow.Ellipsis }).fontSize(11).fontColor('#89958E')
        }.alignItems(HorizontalAlign.Start).layoutWeight(1)
        Button(this.fontMetricsReady ? (this.originalFontProbe.length === 0 ? '测试原版字体' : '再次测试字体') : '字体参数未就绪')
          .id('olex2-original-font-probe').fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .accessibilityDescription(this.fontMetricsReady ? '使用当前窗口实际屏幕DPI' : this.fontMetricsError)
          .enabled(this.graphicsPassed() && this.fontMetricsReady).onClick(() => {
            this.originalFontProbe = nativeBridge.runOriginalFontProbe();
            this.refreshGraphics();
          })
        Button(this.originalTextureLoadProbe.length === 0 ? '加载原版纹理' : '再次加载纹理')
          .id('olex2-original-texture-load-probe').fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed()).onClick(() => {
            this.originalTextureLoadProbe = nativeBridge.runOriginalTextureLoadProbe();
            this.originalLog = nativeBridge.getOriginalLogs();
            this.refreshGraphics();
          })
        Button(this.originalProbeLabel()).id('olex2-original-renderer-probe')
          .fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed()).onClick(() => {
            this.originalRendererProbe = nativeBridge.runOriginalRendererProbe();
            this.refreshGraphics();
          })
      }.width('100%').padding(18)
      Text(this.stateSaveStatus === 'saved' ? (this.stateLoadStatus === 'restored' ? '已恢复上次视图 · 当前已保存' : '当前视图已保存') : this.stateSaveStatus === 'write-error' ? '视图保存失败' : '正在保存视图')
        .id('olex2-workspace-state').fontSize(10).fontColor('#74867A').width('100%').padding({ left: 18, bottom: 6 })
      Stack({ alignContent: Alignment.TopStart }) {
        XComponent({ id: 'olex2-surface', type: XComponentType.SURFACE, libraryname: 'olex2native' })
          .width('100%').height('100%')
          .onLoad(() => this.refreshGraphics())
          .hitTestBehavior(HitTestMode.None)
        // Focusable transparent input plane: native SURFACE owns pixels only.
        Button('').id('olex2-viewport-input').width('100%').height('100%')
          .backgroundColor(Color.Transparent).borderRadius(0).focusable(true).focusOnTouch(true).defaultFocus(true)
          .onClick(() => this.getUIContext().getFocusController().requestFocus('olex2-viewport-input'))
          .onKeyEvent((event: KeyEvent) => this.viewportKey(event))
          .onAxisEvent((event: AxisEvent) => this.viewportAxis(event))
          .onHover((hovered: boolean) => { this.viewportHovered = hovered; this.saveRecord(); })
          .gesture(PanGesture().onActionStart(() => {
            this.dragYaw = this.yaw; this.dragPitch = this.pitch;
          }).onActionUpdate((event: GestureEvent) => {
            this.yaw = this.dragYaw + event.offsetX * 0.35;
            this.pitch = Math.max(-85, Math.min(85, this.dragPitch + event.offsetY * 0.35));
            this.setView();
          }))
        Text(`上游${this.currentPrimitiveLabel()}图元 · ${this.graphics.wireframe ? '线框' : '实体'} · 非结构模型`).fontSize(10).fontColor('#B4CBC0')
          .padding({ left: 10, right: 10, top: 6, bottom: 6 }).backgroundColor('#90203027')
          .borderRadius(5).margin(12).hitTestBehavior(HitTestMode.None)
      }.width('100%').layoutWeight(1).constraintSize({ minHeight: 140 }).backgroundColor('#0E141A')
      Row({ space: 7 }) {
        ForEach([0, 1, 2, 3], (kind: number) => {
          Button(this.primitiveLabel(kind)).id(`olex2-primitive-${kind}`).layoutWeight(1).height(26).fontSize(10)
            .fontColor(this.graphics.meshKind === this.primitiveName(kind) ? '#FFFFFF' : '#3C5F4B')
            .backgroundColor(this.graphics.meshKind === this.primitiveName(kind) ? '#397654' : '#EDF3EF')
            .enabled(this.graphics.initialized).onClick(() => this.selectPrimitive(kind))
        }, (kind: number) => `${kind}`)
        ForEach([0, 1, 2], (preset: number) => {
          Button(this.colorLabel(preset)).id(`olex2-color-${preset}`).height(26).fontSize(10).width(48).padding({ left: 4, right: 4 })
            .fontColor('#122C28').backgroundColor(this.colorSwatch(preset))
            .border({ width: this.graphics.colorPreset === preset ? 2 : 0, color: '#193B2B' })
            .enabled(this.graphics.initialized).onClick(() => this.selectColor(preset))
        }, (preset: number) => `color-${preset}`)
      }.width('100%').padding({ left: 15, right: 15, top: 8 })
      Row({ space: 7 }) {
        Text('材质子集').fontSize(10).fontColor('#7B8D80')
        ForEach([0, 1, 2, 3], (preset: number) => {
          Button(this.materialLabel(preset)).id(`olex2-material-${preset}`).layoutWeight(1).height(24).fontSize(10)
            .padding({ left: 4, right: 4 }).fontColor(this.graphics.materialPreset === preset ? '#FFFFFF' : '#3C5F4B')
            .backgroundColor(this.graphics.materialPreset === preset ? '#397654' : '#EDF3EF')
            .enabled(this.graphics.initialized).onClick(() => this.selectMaterial(preset))
        }, (preset: number) => `material-${preset}`)
      }.width('100%').padding({ left: 15, right: 15, top: 8 })
      Row({ space: 7 }) {
        Button('降低精细度').id('olex2-mesh-less').fontSize(10).height(29)
          .fontColor('#3C5F4B').backgroundColor('#EDF3EF').enabled(this.graphics.initialized && this.graphics.meshGeneration > 0)
          .onClick(() => this.updateMesh(this.graphics.meshGeneration - 1, this.graphics.wireframe))
        Text(`细分 ${this.graphics.meshGeneration}`).fontSize(11).fontColor('#5F7467')
        Button('提高精细度').id('olex2-mesh-more').fontSize(10).height(29)
          .fontColor('#3C5F4B').backgroundColor('#EDF3EF').enabled(this.graphics.initialized && this.graphics.meshGeneration < 5)
          .onClick(() => this.updateMesh(this.graphics.meshGeneration + 1, this.graphics.wireframe))
        Blank()
        Button(this.graphics.wireframe ? '切换实体' : '切换线框').id('olex2-mesh-mode').fontSize(10).height(29)
          .fontColor('#FFFFFF').backgroundColor('#397654').enabled(this.graphics.initialized)
          .onClick(() => this.updateMesh(this.graphics.meshGeneration, !this.graphics.wireframe))
      }.width('100%').padding({ left: 15, right: 15, top: 10, bottom: 3 })
      Text(`${this.graphics.meshVertices} 源顶点 · ${this.graphics.meshTriangles} 三角形 · ${this.graphics.sourceBatches} 批次`)
        .fontSize(10).fontColor('#7B8D80').width('100%').padding({ left: 17, right: 10, top: 5, bottom: 1 })
      Row({ space: 8 }) {
        Button('↶').width(38).height(32).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.rotateView(-20))
        Button('↷').width(38).height(32).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.rotateView(20))
        Button('重置视角').id('olex2-view-reset').height(32).fontSize(11).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.resetView())
        Blank()
        Text('缩放').fontSize(11).fontColor('#89958E')
        Slider({ value: this.distance, min: 2.2, max: 10, step: 0.1 }).id('olex2-view-zoom').width(105)
          .selectedColor('#438563').trackColor('#E4ECE6').onChange((value: number) => { this.distance = value; this.setView(); })
      }.width('100%').padding({ left: 15, right: 15, top: 10, bottom: 10 })
    }.id('olex2-viewport-panel').width('100%').height('100%').backgroundColor('#FFFFFF').borderRadius(14)
      .border({ width: 1, color: '#E3E9E4' }).clip(true)
  }

  build() {
    Row({ space: 0 }) {
      Column() {
        Column({ space: 8 }) {
          Text('Olex2').fontSize(32).fontWeight(FontWeight.Medium).fontColor('#FFFFFF').width('100%')
          Text('H A R M O N Y   P C').fontSize(10).fontColor('#90B6A1').width('100%')
        }.padding({ left: 20, right: 20, top: 30, bottom: 34 }).alignItems(HorizontalAlign.Start)
        Column({ space: 6 }) {
          this.NavItem(0, '◇', '适配工作台')
          this.NavItem(1, '✓', '基础代码检查')
          this.NavItem(2, '≡', '运行记录')
        }.padding({ left: 12, right: 12 })
        this.CommandEntry()
        Blank()
        Column({ space: 10 }) {
          Text(this.packageIdentityError.length > 0 ? this.packageIdentityError :
            this.versionName.length > 0 ? `开发验证版  /  ${this.versionName}` : '正在读取已安装版本')
            .id('olex2-installed-version').fontSize(11).fontColor('#9BB2A6').width('100%')
          Text('原版界面与完整功能尚未移植。此版本仅验证真实基础代码和原生窗口图形链路。')
            .fontSize(11).fontColor('#688977').lineHeight(18).width('100%')
        }.padding(20).alignItems(HorizontalAlign.Start)
      }.width(216).height('100%').backgroundColor('#192D24')
      Column({ space: 18 }) {
        Row() {
          Column({ space: 6 }) {
            Text('鸿蒙 PC  /  原生集成').fontSize(11).fontColor('#7D8C82')
            Text(this.activePanel === 0 ? '适配工作台' : this.activePanel === 1 ? '基础代码检查' : '运行记录')
              .fontSize(26).fontWeight(FontWeight.Medium).fontColor('#263B2D')
          }.alignItems(HorizontalAlign.Start).layoutWeight(1)
          Text('开发验证版').fontSize(11).fontColor('#8B7757').backgroundColor('#F0EBE0')
            .padding({ left: 12, right: 12, top: 7, bottom: 7 }).borderRadius(7)
        }.width('100%')
        this.SummaryCards()
        if (this.activePanel === 0) {
          Row({ space: 16 }) {
            Column() { this.Viewport() }.layoutWeight(3).height('100%')
            Column({ space: 12 }) {
              Row() {
                Text('基础代码检查').fontSize(15).fontWeight(FontWeight.Medium).fontColor('#2E4837').layoutWeight(1)
                Button('重跑').fontSize(11).height(29).fontColor('#397154').backgroundColor('#EBF3ED').onClick(() => this.runChecks())
              }.width('100%')
              this.CheckRows()
              Blank()
              Divider().color('#E8EEE9')
              Text(this.graphics.error.length > 0 ? this.graphics.error :
                `表面 ${this.graphics.width} × ${this.graphics.height}  ·  已提交 ${this.graphics.frames} 帧`)
                .fontSize(11).fontColor(this.graphics.error.length > 0 ? '#AD6C4E' : '#829188').width('100%')
            }.padding(18).layoutWeight(2).height('100%').backgroundColor('#FFFFFF').borderRadius(14)
              .border({ width: 1, color: '#E3E9E4' }).constraintSize({ minWidth: 265 })
          }.width('100%').layoutWeight(1)
        } else if (this.activePanel === 1) {
          Column({ space: 16 }) {
            Row() {
              Column({ space: 5 }) {
                Text('来自真实上游的基础实现').fontSize(17).fontWeight(FontWeight.Medium).fontColor('#2E4837')
                Text('不是预填的通过状态。每项结果均来自 C++ 执行返回。').fontSize(12).fontColor('#849389')
              }.layoutWeight(1).alignItems(HorizontalAlign.Start)
              Button('重新执行检查').height(36).fontSize(12).backgroundColor('#327550').onClick(() => this.runChecks())
            }.width('100%')
            this.CheckRows()
            Blank()
            Text('文件检查只在应用私有目录中独占创建并清理自己的临时文件，不覆盖已有内容。')
              .fontSize(12).fontColor('#849389').width('100%')
          }.padding(24).width('100%').layoutWeight(1).backgroundColor('#FFFFFF').borderRadius(14)
        } else {
          Column({ space: 14 }) {
            this.OptionsDocuments()
            Row() {
              Text('原生运行记录').fontSize(17).fontWeight(FontWeight.Medium).fontColor('#2E4837').layoutWeight(1)
              Button('刷新记录').height(32).fontSize(11).fontColor('#397154').backgroundColor('#EBF3ED').onClick(() => this.refreshGraphics())
            }.width('100%')
            Row({ space: 8 }) {
              Button('平台记录').id('olex2-platform-records').height(28).fontSize(11)
                .onClick(() => { this.originalLogView = false; this.saveRecord(); })
              Button('原版日志').id('olex2-original-logs').height(28).fontSize(11)
                .onClick(() => { this.originalLog = nativeBridge.getOriginalLogs(); this.originalLogView = true; this.saveRecord(); })
            }.width('100%')
            if (this.originalLogView) {
              Row({ space: 8 }) {
                Button('列出原版命令').id('olex2-core-macros').height(30).fontSize(11)
                  .onClick(() => { this.showCoreCatalog('core.LstMac'); })
                Button('列出原版函数').id('olex2-core-functions').height(30).fontSize(11)
                  .onClick(() => { this.showCoreCatalog('core.LstFun'); })
                Button('清空日志').id('olex2-core-log-clear').height(30).fontSize(11)
                  .onClick(() => { this.clearOriginalLog(); })
              }.width('100%')
              Text(this.coreCatalog.error.length > 0 ? this.coreCatalog.error :
                `原始通知 ${this.originalLog.posted} · 保留 ${this.originalLog.entryCount} · 淘汰 ${this.originalLog.dropped}；目录只展示注册信息，不表示全部可用`)
                .id('olex2-core-log-status').fontSize(11).fontColor('#738078').maxLines(3).width('100%')
              Scroll() {
                Text(this.originalLog.entryCount > 0 ? this.originalLogText() : '本次会话暂无原版日志')
                  .id('olex2-core-log-text').fontSize(12).fontFamily('monospace').fontColor('#385140').width('100%')
              }.width('100%').layoutWeight(1)
              Text('缓冲最多256条/64KiB；不自动恢复或重放。仅观察原TLog通知，不含直接流输出或级别。诊断摘要另存私有记录。')
                .fontSize(10).fontColor('#849389').width('100%').maxLines(3)
            } else {
              Scroll() {
                Text(this.lastRecord).fontSize(12).fontFamily('monospace').fontColor('#385140').width('100%')
              }.width('100%').layoutWeight(1)
            }
            Text(this.recordError.length > 0 ? this.recordError : '记录保存于应用私有目录；fullPortComplete 始终反映当前未完成状态。')
              .fontSize(11).fontColor('#849389').width('100%')
          }.padding(24).width('100%').layoutWeight(1).backgroundColor('#FFFFFF').borderRadius(14)
        }
        Text('范围说明：上游图元与提交循环已接入；显示适配为新实现，完整原版渲染器与功能尚未移植。')
          .fontSize(10).fontColor('#89958E').width('100%')
      }.padding(24).layoutWeight(1).height('100%').backgroundColor('#F4F7F3')
    }.width('100%').height('100%')
  }
}
