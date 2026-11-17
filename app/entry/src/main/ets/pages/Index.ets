import nativeBridge, { GraphicsStatus, UtilityReport, CheckResult, ApplicationRuntimeStatus, OriginalLogSnapshot, CoreCatalogReport, PythonRuntimeStatus, OriginalLinePathGpuProbe, PythonExecutionReport, ApplicationFunctionReport, OriginalCommandReport, OriginalGenerateDialogReport, OriginalPeriodicTableReport, OriginalMaterialDialogReport, OriginalSceneDialogReport, OriginalGradientDialogReport, OriginalPrimitiveDialogReport, OriginalUpdateOptionsDialogReport, OriginalBondRadiusReport, OriginalBondLengthDialogReport, OriginalSelectionLabelReport, OriginalBadReflectionsReport, DesktopCommandReport } from 'libolex2native.so';
import { fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { OptionsDocumentTransfer, OptionsDocumentStatus, OptionsDocumentBackend, emptyOptionsDocumentStatus } from '../model/OptionsDocumentTransfer';
import { HarmonyOptionsDocumentPort } from '../model/OptionsDocumentStore';
import { PythonDocumentTransfer, PythonDocumentStatus, emptyPythonDocumentStatus, encodePythonDocument } from '../model/PythonDocument';
import { HarmonyPythonDocumentPort } from '../model/PythonDocumentStore';
import { PythonEditor } from './PythonEditor';
import { OriginalDocumentTextEditor } from './OriginalDocumentEditor';
import OriginalDocumentTabs from './OriginalDocumentTabs';
import { NativeViewportZoom } from './NativeViewportZoom';
import { OriginalDocumentSessions, OriginalDocumentSessionSummary, OriginalDocumentSessionsStatus,
  OriginalDocumentActivation } from '../model/OriginalDocumentSessions';
import { OriginalDocumentEditor, OriginalDocumentEditorStatus, decodeOriginalDocumentText,
  encodeOriginalDocumentText } from '../model/OriginalDocumentEditor';
import { OriginalDocumentRecoveryPanel } from './OriginalDocumentRecoveryPanel';
import { OriginalDocumentRecoverySnapshot, OriginalDocumentRecoveryDocument } from '../model/OriginalDocumentRecovery';
import { OriginalDocumentViewportState, parseOriginalDocumentViewport, copyOriginalDocumentViewport,
  originalDocumentViewportMatchesSource, equalOriginalDocumentViewport } from '../model/OriginalDocumentViewport';
import { OriginalDocumentRecoveryJournal, OriginalDocumentRecoveryJournalStatus } from '../model/OriginalDocumentRecoveryJournal';
import { HarmonyOriginalDocumentRecoveryPort } from '../model/OriginalDocumentRecoveryStore';
import { registerOriginalDocumentRecoveryFlush, unregisterOriginalDocumentRecoveryFlush } from '../model/OriginalDocumentRecoveryLifecycle';
import { OriginalTextHistory, OriginalTextHistoryStatus, LiteralTextMatch, LiteralTextReplacement,
  findLiteralText, findPreviousLiteralText, LiteralTextLineTarget, literalTextLineTarget,
  replaceLiteralMatch, replaceAllLiteralText } from '../model/OriginalTextTools';
import { resolveOriginalDocumentShortcut } from '../model/OriginalDocumentCommands';
import { adjacentDesktopDocumentId, desktopDocumentMenuLabel } from '../model/DesktopDocumentNavigation';
import { desktopMenuButtonWidth, enabledDesktopMenuIds, resolveDesktopMenuNavigation,
  desktopMenuFocusTarget, desktopMenuScrollOffset } from '../model/DesktopMenuNavigation';
import { OriginalDocumentTransfer, OriginalDocumentEnvelope, OriginalDocumentTransferStatus,
  OriginalDocumentTransferResult, OriginalDocumentBackend, OriginalDocumentCoreStatus, emptyOriginalDocument,
  emptyOriginalDocumentTransfer, parseOriginalDocument, originalDocumentPresented,
  ORIGINAL_MAIN_GUI_LABELS_SOURCE, ORIGINAL_MAIN_GUI_STRUCTURE_SOURCE, ORIGINAL_MAIN_GUI_HYDROGEN_SOURCE,
  ORIGINAL_MAIN_GUI_QPEAK_SOURCE, ORIGINAL_MAIN_GUI_CELL_SOURCE, ORIGINAL_MAIN_GUI_BASIS_SOURCE,
  ORIGINAL_MAIN_GUI_FRAME_SOURCE, ORIGINAL_MAIN_GUI_GRID_SOURCE, ORIGINAL_MAIN_GUI_FRAGMENT_SOURCE } from '../model/OriginalDocumentTransfer';
import { HarmonyOriginalDocumentPort } from '../model/OriginalDocumentStore';
import { OriginalDocumentSaveTargets, OriginalDocumentSaveTarget } from '../model/OriginalDocumentSaveTargets';
import { OriginalDocumentSaveTransport, OriginalDocumentSerializedSnapshot } from '../model/OriginalDocumentSaveTransport';
import { originalDocumentSha256 } from '../model/OriginalDocumentHash';
import { OriginalViewportInput, OriginalSelectionRequest, OriginalViewportKeyResult, OriginalViewportDrag,
  originalSelectionRequest, originalSelectionMode, originalVisibilityStateChanged } from '../model/OriginalViewportInput';
import { KeyCode } from '@kit.InputKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { ViewportLayoutBudget, viewportLayoutBudget } from '../model/ViewportLayout';
import { DesktopState, DesktopStateSaveQueue } from '../model/DesktopState';
import { loadDesktopState, writeDesktopState } from '../model/DesktopStateStore';
import { usesApplicationInterpreter } from '../model/CommandSubmit';
import { resolveDesktopShortcut, DesktopShortcutLatch, desktopContentWidth, compactDesktopSummary, showDesktopChecksPanel, compactDesktopSidebar, desktopHelpHeight } from '../model/DesktopActions';

interface WorkspaceStateStatus {
  restored: boolean;
  loadStatus: string;
  saveStatus: string;
  schemaVersion: number;
  readbackStatus: string;
  readbackState?: DesktopState;
  uiYaw: number; uiPitch: number; uiDistance: number;
}

interface DesktopCommandStatus {
  sequence: number; input: string; passed: boolean; changed: boolean; applied: boolean; dispatched: number;
  source: string; message: string; error: string;
}

interface ApplicationFunctionStatus {
  sequence: number; input: string; passed: boolean; optionsChanged: boolean; text: string; error: string; source: string;
}
interface PythonExecutionStatus {
  sequence: number; source: string; passed: boolean; optionsChanged: boolean; output: string; error: string;
}
interface PythonEditorStatus {
  visible: boolean; dirty: boolean; busy: boolean; characters: number; document: PythonDocumentStatus;
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

interface OriginalTextToolsAudit {
  history: OriginalTextHistoryStatus;
  findVisible: boolean; goToLineVisible: boolean; goToLineCharacters: number;
  sourceFocused: boolean; queryCharacters: number; replacementCharacters: number;
  selectionStart: number; selectionEnd: number; action: string; sequence: number; replaceCount: number;
  shortcutAction: string; shortcutSequence: number;
}

// Each tab owns real editor/history instances. Switching never resets or serializes them.
interface OriginalDocumentSessionUi {
  id: string; editor: OriginalDocumentEditor; history: OriginalTextHistory;
  editorVisible: boolean; findVisible: boolean; query: string; replacement: string;
  selectionStart: number; selectionEnd: number; message: string; sourceKind: string;
  toolsAction: string; toolsSequence: number; replaceCount: number;
  viewport?: OriginalDocumentViewportState; viewportSummary?: OriginalDocumentViewportSummary;
  viewportSequence: number; viewportObservationKey: string;
}
interface OriginalDocumentViewportOwnerCore extends OriginalDocumentCoreStatus {
  contextRetained?: boolean;
}
interface OriginalDocumentViewportSummary {
  name: string; byteCount: number; sha256: string; yaw: number; pitch: number; distance: number;
  objectCount: number; inventorySha256: string; selectedObjectCount: number; selectedIndicesSha256: string;
}
interface OriginalDocumentViewportAudit {
  sequence: number; documentId: string; action: string; state: string; error: string;
  captureCount: number; restoreCount: number; defaultCount: number;
}
interface OriginalDocumentSessionEditorAudit {
  id: string; editor: OriginalDocumentEditorStatus; history: OriginalTextHistoryStatus;
  editorVisible: boolean; findVisible: boolean; queryCharacters: number; replacementCharacters: number;
  selectionStart: number; selectionEnd: number; viewportSequence: number; viewport?: OriginalDocumentViewportSummary;
}
interface OriginalDocumentSessionOperation {
  sequence: number; operation: string; targetId: string; state: string; error: string;
}

interface OriginalDocumentSaveAudit {
  sequence: number; id: string; mode: string; state: string; selectedUri: string;
  name: string; byteCount: number; sha256: string; writeStarted: boolean; error: string;
}
interface OriginalDocumentSaveCompletion {
  id: string; mode: string; snapshot: OriginalDocumentSerializedSnapshot; nativeSource: OriginalDocumentEnvelope;
}

interface OriginalDocumentRecoveryAudit {
  state: string; pendingDecision: boolean; busy: boolean; restoredDocumentCount: number; temporaryNativeRetained: boolean;
  action: string; actionSequence: number; detail: string; error: string;
  journal: OriginalDocumentRecoveryJournalStatus;
}
interface OriginalDocumentRecoveryPresented {
  document: OriginalDocumentEnvelope; bytes: Uint8Array;
}

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
  pythonRuntime: PythonRuntimeStatus;
  pythonExecution: PythonExecutionStatus;
  pythonEditor: PythonEditorStatus;
  optionsDocument: OptionsDocumentStatus;
  originalDocument: OriginalDocumentEnvelope;
  originalDocumentSessions: OriginalDocumentSessionsStatus;
  originalDocumentSaveTargets: OriginalDocumentSaveTarget[];
  originalDocumentSaveOperation: OriginalDocumentSaveAudit;
  originalDocumentRecovery: OriginalDocumentRecoveryAudit;
  originalDocumentSessionTabs: OriginalDocumentSessionSummary[];
  originalDocumentSessionEditors: OriginalDocumentSessionEditorAudit[];
  originalDocumentViewport: OriginalDocumentViewportAudit;
  originalDocumentSessionOperation: OriginalDocumentSessionOperation;
  originalDocumentEditorGuardTargetId: string;
  originalDocumentTransfer: OriginalDocumentTransferStatus;
  originalDocumentLastOperation: OriginalDocumentEnvelope;
  originalDocumentEditor: OriginalDocumentEditorStatus;
  originalDocumentEditorVisible: boolean;
  originalDocumentTextTools: OriginalTextToolsAudit;
  originalDocumentEditorMessage: string;
  originalDocumentEditorGuardBusy: boolean;
  originalDocumentEditorGuardDecision: string;
  originalDocumentEditorGuardSequence: number;
  originalDocumentSourceKind: string;
  originalDocumentStatusError: string;
  originalSelectionMessage: string; originalSelectionBusy: boolean;
  originalViewportFocused: boolean; originalSelectionMode: number;
  originalLog: OriginalLogAudit;
  coreCatalog: CoreCatalogStatus;
  originalLogView: boolean;
}

interface OriginalRecentDocument {
  key: string;
  name: string;
  bytes: Uint8Array;
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
  // Original Olex2 main frame has no left sidebar; the adapter sidebar starts hidden
  // and stays reachable from 视图 / 顶部“显示侧栏”.
  @State private sidebarVisible: boolean = false;
  private sidebarScroller: Scroller = new Scroller();
  private sidebarContentHeight: number = 0;
  private sidebarViewportHeight: number = 0;
  private sidebarCommandFocusPending: boolean = false;
  @State private desktopWidth: number = 1200;
  @State private desktopHeight: number = 900;
  @State private viewportPanelHeight: number = 0;
  @State private viewportToolsExpanded: boolean = false;
  @State private shortcutHelpVisible: boolean = false;
  @State private aboutVisible: boolean = false;
  private desktopMenuScroller: Scroller = new Scroller();
  private desktopMenuViewportWidth: number = 0;
  private desktopMenuFocused: boolean = false;
  private focusedDesktopMenuId: string = '';
  private desktopMenuReturnControl: string = 'source';
  private desktopShortcutLatch: DesktopShortcutLatch = new DesktopShortcutLatch();
  private desktopMetaKeys: Set<number> = new Set<number>();
  @State private cmdLineText: string = '';
  @State private commandText: string = 'pc.help';
  @State private commandHistory: string[] = [];
  private commandHistoryIndex: number = -1;
  @State private originalBondRadiusText: string = '0.2';
  @State private originalRecentLabels: string[] = [];
  private originalRecentList: OriginalRecentDocument[] = [];
  private generateDialogTimer: number = -1;
  private generateDialogOpen: boolean = false;
  private periodicTableTimer: number = -1;
  private periodicTableOpen: boolean = false;
  private materialDialogTimer: number = -1;
  private materialDialogOpen: boolean = false;
  private sceneDialogTimer: number = -1;
  private sceneDialogOpen: boolean = false;
  private originalTextApplyRebindPending: boolean = false;
  private gradientDialogTimer: number = -1;
  private gradientDialogOpen: boolean = false;
  private createOriginalSelectionLabel(): void {
    if (!this.originalSelectionEnabled()) return;
    try {
      const result: OriginalSelectionLabelReport = nativeBridge.createOriginalSelectionLabel('');
      if (!result.passed) {
        this.originalSelectionMessage = result.error || '原版选择组标签未创建';
        return;
      }
      this.originalSelectionMessage = `已创建原版选择组标签：${result.text}`;
      try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版选择组标签调用失败';
    }
  }

  private bondLengthDialogTimer: number = -1;
  private bondLengthDialogOpen: boolean = false;
  private updateOptionsDialogTimer: number = -1;
  private updateOptionsDialogOpen: boolean = false;
  private primitiveDialogTimer: number = -1;
  private primitiveDialogOpen: boolean = false;
  @State private commandMessage: string = '原版解析器已接入视图命令；完整命令库未接通';
  @State private commandError: string = '';
  @State private commandPassed: boolean = false;
  private commandStatus: DesktopCommandStatus = { sequence: 0, input: '', passed: false, changed: false, applied: false, dispatched: 0, source: '', message: '', error: '' };
  @State private applicationRuntime: ApplicationRuntimeStatus = { initialized: false, optionsRestored: false, licenseVerified: false, commandHints: true, lastSaveCommitted: false, originalCoreInitialized: false, originalCoreEmptyFile: false, originalCoreMacroCount: 0, originalCoreFunctionCount: 0, originalCoreFileCount: 0, originalFunctionCount: 0, optionsCount: 0, saveRevision: 0, baseDir: '', instanceDir: '', sharedDir: '', configDir: '', licenseSha256: '', optionsSha256: '', platform: '', error: '', source: '' };
  private applicationFunction: ApplicationFunctionStatus = { sequence: 0, input: '', passed: false, optionsChanged: false, text: '', error: '', source: '' };
  @State private pythonRuntime: PythonRuntimeStatus = { prepared: false, initialized: false, originalBridgeImported: false, executions: 0, version: '', home: '', stdlibSha256: '', error: '' };
  @State private pythonExecution: PythonExecutionStatus = { sequence: 0, source: '', passed: false, optionsChanged: false, output: '', error: '' };
  @State private pythonEditorVisible: boolean = false;
  @State private pythonEditorSource: string = '';
  @State private pythonEditorDirty: boolean = false;
  @State private pythonTransferBusy: boolean = false;
  @State private pythonEditorMessage: string = 'UTF-8 文件上限 64 KiB。关闭编辑器保留会话草稿，重启前请另存为。';
  @State private pythonDocument: PythonDocumentStatus = emptyPythonDocumentStatus();
  private pythonTransfer: PythonDocumentTransfer = new PythonDocumentTransfer();
  @State private originalLogView: boolean = false;
  @State private originalLog: OriginalLogSnapshot = { attached: false, posted: 0, dropped: 0, truncated: 0, captureFailures: 0, clearCount: 0, clearedEntries: 0, entryCount: 0, retainedBytes: 0, textSha256: '', source: '', error: '', entries: [] };
  private coreCatalog: CoreCatalogStatus = { sequence: 0, passed: false, exposedMacros: 0, postedBefore: 0, postedAfter: 0, input: '', error: '', source: '' };
  @State private originalDocument: OriginalDocumentEnvelope = emptyOriginalDocument();
  private originalDocumentLastOperation: OriginalDocumentEnvelope = emptyOriginalDocument();
  @State private originalDocumentTransferStatus: OriginalDocumentTransferStatus = emptyOriginalDocumentTransfer();
  @State private originalDocumentBusy: boolean = false;
  @State private originalSelectionBusy: boolean = false;
  @State private originalViewportFocused: boolean = false;
  @State private originalSelectionMode: number = 0;
  @State private originalSelectionMessage: string = '尚未进行选择；数量由原生选择组直接返回。';
  private originalViewportInput: OriginalViewportInput = new OriginalViewportInput();
  @State private originalDocumentMessage: string = '每份 UTF-8 文档上限 8 MiB；本次会话最多 4 份、合计 32 MiB；切换保留草稿，私有会话备份需明确恢复；不是完整原界面。';
  @State private originalDocumentStatusError: string = '';
  @State private originalDocumentSourceKind: string = '';
  private originalDocumentRaw: string = '';
  private originalDocumentTransfer: OriginalDocumentTransfer = new OriginalDocumentTransfer();
  private originalDocumentSessions: OriginalDocumentSessions = new OriginalDocumentSessions();
  private originalDocumentSaveTargets: OriginalDocumentSaveTargets = new OriginalDocumentSaveTargets();
  @State private originalDocumentSaveTargetSummaries: OriginalDocumentSaveTarget[] = [];
  private originalDocumentSaveOperation: OriginalDocumentSaveAudit = { sequence: 0, id: '', mode: '',
    state: 'idle', selectedUri: '', name: '', byteCount: 0, sha256: '', writeStarted: false, error: '' };
  private originalDocumentSessionUi: Map<string, OriginalDocumentSessionUi> = new Map<string, OriginalDocumentSessionUi>();
  private boundOriginalDocumentId: string = '';
  private originalDocumentViewportAudit: OriginalDocumentViewportAudit = { sequence: 0, documentId: '', action: '',
    state: 'idle', error: '', captureCount: 0, restoreCount: 0, defaultCount: 0 };
  private originalRecoveryJournal: OriginalDocumentRecoveryJournal | undefined = undefined;
  private originalRecoveryLoaded: boolean = false;
  private originalRecoveryTimer: number = -1;
  private originalRecoveryLifecycleToken: number = 0;
  private originalRecoveryCheckpointKey: string = '';
  private originalRecoveryScheduledKey: string = '';
  private originalRecoveryTemporaryNative: boolean = false;
  @State private originalRecoveryState: string = 'unavailable';
  @State private originalRecoveryPending: boolean = false;
  @State private originalRecoveryCanRestore: boolean = false;
  @State private originalRecoveryBusy: boolean = false;
  @State private originalRecoveryPanelVisible: boolean = false;
  @State private originalRecoveryMessage: string = '变更后约 250 ms 备份到应用私有目录；不替代保存，不保证突发终止前最后一笔修改已备份。';
  @State private originalRecoveryError: string = '';
  @State private originalRecoveryStatus: OriginalDocumentRecoveryJournalStatus = {
    state: 'not-loaded', generation: 0, selectedSlot: -1, documentCount: 0, savedAt: '', byteCount: 0,
    sha256: '', error: '', warning: '', writeSequence: 0, validSlots: 0, invalidSlots: 0 };
  private originalRecoveryRestoredCount: number = 0;
  private originalRecoveryAction: string = 'not-loaded';
  private originalRecoveryActionSequence: number = 0;
  @State private originalDocumentTabs: OriginalDocumentSessionSummary[] = [];
  @State private originalDocumentSessionsStatus: OriginalDocumentSessionsStatus = {
    count: 0, activeId: '', nativeSessionId: '', pendingId: '', totalSourceBytes: 0,
    maximumDocuments: 4, maximumSourceBytes: 32 * 1024 * 1024, sequence: 0, error: '' };
  private originalDocumentSessionOperation: OriginalDocumentSessionOperation = {
    sequence: 0, operation: '', targetId: '', state: 'idle', error: '' };
  private originalTextGuardTargetId: string = '';
  private originalTextEditor: OriginalDocumentEditor = new OriginalDocumentEditor();
  private originalTextHistory: OriginalTextHistory = new OriginalTextHistory();
  private originalDocumentShortcutLatch: DesktopShortcutLatch = new DesktopShortcutLatch();
  private desktopDocumentNavigationLatch: DesktopShortcutLatch = new DesktopShortcutLatch();
  @State private originalTextCanUndo: boolean = false;
  @State private originalTextCanRedo: boolean = false;
  @State private originalTextFindVisible: boolean = false;
  // Line navigation is transient; it is never carried into another document.
  @State private originalTextGoToLineVisible: boolean = false;
  @State private originalTextGoToLineInput: string = '';
  @State private originalTextQuery: string = '';
  @State private originalTextReplacement: string = '';
  @State private originalTextSelectionStart: number = 0;
  @State private originalTextSelectionEnd: number = 0;
  @State private originalTextSelectionSequence: number = 0;
  @State private originalTextFocusTarget: string = '';
  @State private originalTextFocusSequence: number = 0;
  private originalTextFocusedControl: string = '';
  private originalTextToolsAction: string = '';
  private originalTextToolsSequence: number = 0;
  private originalTextReplaceCount: number = 0;
  private originalDocumentShortcutAction: string = '';
  private originalDocumentShortcutSequence: number = 0;
  @State private originalTextEditorVisible: boolean = false;
  @State private originalTextEditorSource: string = '';
  @State private originalTextEditorState: OriginalDocumentEditorStatus = { initialized: false, name: '',
    baseRevision: 0, baseSha256: '', baseBytes: 0, draftCharacters: 0, draftDirty: false,
    documentDirty: false, needsSave: false, sequence: 0, error: '' };
  @State private originalTextEditorMessage: string = '实际源文本；另存为经原版保存器规范化。';
  @State private originalTextGuardBusy: boolean = false;
  private originalTextGuardDecision: string = '';
  private originalTextGuardSequence: number = 0;
  private optionsTransfer: OptionsDocumentTransfer = new OptionsDocumentTransfer();
  @State private optionsDocument: OptionsDocumentStatus = emptyOptionsDocumentStatus();
  @State private optionsTransferBusy: boolean = false;
  @State private checks: UtilityReport = { passed: false, checks: [], error: '' };
  @State private graphics: GraphicsStatus = {
    surfaceReady: false, initialized: false, pixelVerified: false, geometryVerified: false,
    coverageVerified: false, meshValidated: false, wireframe: false, meshGeneration: 3, meshRevision: 0,
    submissionValidated: false, submittedVertices: 0, submissionSource: '',
    originalPolygonVerified: false, originalPolygonBatches: 0, originalPolygonVertices: 0, originalPolygonNormalCommands: 0, originalPolygonColorCommands: 0, originalPolygonSource: '',
    linePathProbe: { verified: false, stateRestored: false, contextRestored: false, source: '', error: '', cases: [] } as OriginalLinePathGpuProbe,
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
  private desktopReadbackStatus: string = 'not-read';
  private desktopReadbackState: DesktopState | undefined = undefined;

  aboutToAppear(): void {
    this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
    this.pythonRuntime = nativeBridge.getPythonRuntimeStatus();
    this.originalLog = nativeBridge.getOriginalLogs();
    this.readOriginalDocumentStatus();
    if (!this.originalDocumentActive()) this.restoreDesktopState();
    this.loadOriginalDocumentRecovery();
    this.originalRecoveryLifecycleToken = registerOriginalDocumentRecoveryFlush(() => this.flushOriginalDocumentRecovery(false));
    // Ordinary startup does not rerun the historical diagnostic suite.
    this.saveRecord();
    this.timer = setInterval(() => this.refreshGraphics(), 800);
  }

  aboutToDisappear(): void {
    this.desktopMenuFocused = false; this.desktopMenuViewportWidth = 0;
    this.flushOriginalDocumentRecovery(false);
    unregisterOriginalDocumentRecoveryFlush(this.originalRecoveryLifecycleToken);
    if (this.originalRecoveryTimer >= 0) clearTimeout(this.originalRecoveryTimer);
    this.originalRecoveryTimer = -1;
    if (this.stateTimer >= 0) clearTimeout(this.stateTimer);
    this.stateTimer = -1;
    this.flushDesktopState();
    if (this.timer >= 0) clearInterval(this.timer);
    this.timer = -1;
    if (this.generateDialogTimer >= 0) clearInterval(this.generateDialogTimer);
    this.generateDialogTimer = -1;
    if (this.periodicTableTimer >= 0) clearInterval(this.periodicTableTimer);
    this.periodicTableTimer = -1;
    if (this.materialDialogTimer >= 0) clearInterval(this.materialDialogTimer);
    this.materialDialogTimer = -1;
    if (this.sceneDialogTimer >= 0) clearInterval(this.sceneDialogTimer);
    this.sceneDialogTimer = -1;
    if (this.gradientDialogTimer >= 0) clearInterval(this.gradientDialogTimer);
    this.gradientDialogTimer = -1;
    if (this.primitiveDialogTimer >= 0) clearInterval(this.primitiveDialogTimer);
    this.primitiveDialogTimer = -1;
    if (this.updateOptionsDialogTimer >= 0) clearInterval(this.updateOptionsDialogTimer);
    this.updateOptionsDialogTimer = -1;
    if (this.bondLengthDialogTimer >= 0) clearInterval(this.bondLengthDialogTimer);
    this.bondLengthDialogTimer = -1;
  }

  private submitCommand(): void {
    const input: string = this.commandText.trim();
    if (input.startsWith('py ')) this.executePythonScript();
    else if (usesApplicationInterpreter(this.commandText)) this.executeApplicationFunction();
    else if (input.startsWith('pc.')) this.executeDesktopCommand();
    else this.executeOriginalCommand();
  }

  private executePythonScript(): void {
    const input: string = this.commandText.trim();
    const source: string = input.startsWith('py ') ? input.substring(3) : input;
    this.executePythonSource(source);
  }

  private executePythonSource(source: string): void {
    const sequence: number = this.pythonExecution.sequence + 1;
    try {
      const result: PythonExecutionReport = nativeBridge.executePythonScript(source);
      this.pythonRuntime = result.runtime;
      this.pythonExecution = { sequence: sequence, source: source, passed: result.passed,
        optionsChanged: result.optionsChanged, output: result.output, error: result.error };
      this.commandPassed = result.passed;
      this.commandError = result.error;
      this.commandMessage = result.output.length > 0 ? result.output : (result.passed ? 'Python 脚本已执行' : 'Python 执行未通过');
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
    } catch (error) {
      this.commandPassed = false;
      this.commandError = 'Python 调用失败；请检查运行环境状态';
      this.pythonExecution = { sequence: sequence, source: source, passed: false,
        optionsChanged: false, output: '', error: this.commandError };
      this.pythonRuntime = nativeBridge.getPythonRuntimeStatus();
    }
    this.graphics = nativeBridge.getGraphicsStatus();
    this.originalLog = nativeBridge.getOriginalLogs();
    this.saveRecord();
  }

  private openPythonEditor(): void {
    this.originalViewportInput.setFocused(false);
    this.originalViewportFocused = false;
    this.originalTextEditorVisible = false;
    this.pythonEditorVisible = true;
    this.desktopShortcutLatch.reset(); this.desktopMetaKeys.clear();
    this.saveRecord();
  }

  private confirmPythonReplacement(action: () => void): void {
    if (this.pythonTransferBusy) return;
    if (!this.pythonEditorDirty) { action(); return; }
    AlertDialog.show({ title: '草稿有未保存修改', message: '继续会替换当前草稿。需要保留时请先取消，并选择“另存为”。',
      primaryButton: { value: '取消', action: () => {} },
      secondaryButton: { value: '继续', action: action } });
  }

  private newPythonDocument(): void {
    this.confirmPythonReplacement(() => {
      this.pythonEditorSource = ''; this.pythonEditorDirty = false;
      this.pythonDocument = emptyPythonDocumentStatus();
      this.pythonEditorMessage = '新草稿仅保留本次会话，重启前请另存为。';
      this.saveRecord();
    });
  }

  private async transferPythonDocument(operation: string): Promise<void> {
    if (this.pythonTransferBusy) return;
    this.pythonTransferBusy = true;
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const result = await this.pythonTransfer.run(operation, this.pythonEditorSource, new HarmonyPythonDocumentPort(context));
      this.pythonDocument = result.status;
      if (result.status.state === 'passed') {
        this.pythonEditorSource = result.source;
        this.pythonEditorDirty = false;
        this.pythonEditorMessage = operation === 'import' ?
          `已载入 ${result.status.byteCount} 字节，尚未执行。确认内容后点击“运行脚本”。` :
          `已保存并回读核对 ${result.status.byteCount} 字节。`;
      } else if (result.status.state === 'cancelled') {
        this.pythonEditorMessage = '已取消文件选择，当前草稿未改变。';
      } else {
        this.pythonEditorMessage = result.status.error + (result.status.destinationMayBePartial ? ' 目标文件可能未写完，请检查后重试。' : ' 当前草稿未改变。');
      }
    } catch (error) {
      this.pythonEditorMessage = '系统文件操作未完成，当前草稿保留。';
    } finally { this.pythonTransferBusy = false; this.saveRecord(); }
  }

  private originalDocumentOperationBusy(): boolean {
    return this.originalDocumentBusy || this.originalSelectionBusy || this.originalTextGuardBusy ||
      this.originalRecoveryPending || this.originalRecoveryBusy;
  }

  private viewportInputAllowed(): boolean {
    return !this.originalDocumentOperationBusy() && !this.optionsTransferBusy && !this.pythonTransferBusy &&
      !this.pythonEditorVisible && !this.originalTextEditorVisible && this.graphics.surfaceReady && this.graphics.initialized &&
      (this.originalDocumentActive() ? this.originalDocumentViewPresented() : this.graphics.error.length === 0);
  }

  private originalSelectionAvailable(): boolean {
    return !this.pythonEditorVisible && !this.originalTextEditorVisible && this.originalDocumentViewPresented();
  }

  private syncOriginalViewportInput(): void {
    this.originalViewportInput.setContext(!this.pythonEditorVisible && !this.originalTextEditorVisible && this.graphics.surfaceReady && this.graphics.initialized &&
      (this.originalDocumentActive() ? this.originalDocumentViewPresented() : this.graphics.error.length === 0),
      this.originalDocumentOperationBusy() || this.optionsTransferBusy || this.pythonTransferBusy);
  }

  private originalSelectionEnabled(): boolean {
    return this.originalSelectionAvailable() && this.viewportInputAllowed();
  }

  private selectOriginalDocument(action: string): void {
    this.performOriginalSelection(originalSelectionRequest(action, 0, 0, this.originalSelectionMode));
  }

  private performOriginalSelection(request: OriginalSelectionRequest): void {
    if (!this.originalSelectionEnabled()) return;
    this.originalSelectionBusy = true;
    this.syncOriginalViewportInput();
    const previousCore: OriginalDocumentCoreStatus = this.originalDocument.core;
    const previousSequence: number | undefined = previousCore.selectionSequence;
    let previousVisibility: boolean | undefined;
    try {
      // The page already imports the actual NAPI bridge directly. Do not add a
      // second wrapper or silently fall back to demo/local selection state.
      const validated: OriginalSelectionRequest = originalSelectionRequest(request.action, request.x, request.y, request.mode);
      const visibilityAction: boolean = validated.action === 'toggle-cell' || validated.action === 'toggle-basis' ||
        validated.action === 'toggle-frame' || validated.action === 'toggle-grid' || validated.action === 'toggle-structure' ||
        validated.action === 'toggle-labels' || validated.action === 'toggle-hydrogens' ||
        validated.action === 'toggle-hydrogen-bonds' || validated.action === 'toggle-qpeaks' ||
        validated.action === 'toggle-qpeak-bonds';
      if (validated.action === 'toggle-cell') previousVisibility = previousCore.cellVisible;
      else if (validated.action === 'toggle-basis') previousVisibility = previousCore.basisVisible;
      else if (validated.action === 'toggle-frame') previousVisibility = previousCore.frameVisible;
      else if (validated.action === 'toggle-grid') previousVisibility = previousCore.gridVisible;
      else if (validated.action === 'toggle-structure') previousVisibility = previousCore.structureVisible;
      else if (validated.action === 'toggle-labels') previousVisibility = previousCore.labelsVisible;
      else if (validated.action === 'toggle-hydrogens') previousVisibility = previousCore.hydrogensVisible;
      else if (validated.action === 'toggle-hydrogen-bonds') previousVisibility = previousCore.hydrogenBondsVisible;
      else if (validated.action === 'toggle-qpeaks') previousVisibility = previousCore.qPeaksVisible;
      else if (validated.action === 'toggle-qpeak-bonds') previousVisibility = previousCore.qPeakBondsVisible;
      const raw: string = nativeBridge.interactOriginalDocument(validated.action, validated.x, validated.y, validated.mode);
      const document: OriginalDocumentEnvelope = parseOriginalDocument(raw);
      this.originalDocumentLastOperation = document;
      this.originalDocument = document;
      this.originalDocumentRaw = raw;
      if (!originalDocumentPresented(document)) {
        throw new Error(document.error || document.core.error || '选择后原版视口未确认呈现；可关闭文档。');
      }
      const core = document.core;
      let actualVisibility: boolean | undefined;
      if (validated.action === 'toggle-cell') actualVisibility = core.cellVisible;
      else if (validated.action === 'toggle-basis') actualVisibility = core.basisVisible;
      else if (validated.action === 'toggle-frame') actualVisibility = core.frameVisible;
      else if (validated.action === 'toggle-grid') actualVisibility = core.gridVisible;
      else if (validated.action === 'toggle-structure') actualVisibility = core.structureVisible;
      else if (validated.action === 'toggle-labels') actualVisibility = core.labelsVisible;
      else if (validated.action === 'toggle-hydrogens') actualVisibility = core.hydrogensVisible;
      else if (validated.action === 'toggle-hydrogen-bonds') actualVisibility = core.hydrogenBondsVisible;
      else if (validated.action === 'toggle-qpeaks') actualVisibility = core.qPeaksVisible;
      else if (validated.action === 'toggle-qpeak-bonds') actualVisibility = core.qPeakBondsVisible;
      if (visibilityAction && !originalVisibilityStateChanged(previousVisibility, actualVisibility)) {
        throw new Error('未收到原生可见性状态变化的读回，不能确认本次切换。');
      }
      if (core.selectedObjectCount === undefined || core.selectionSequence === undefined || core.pickHit === undefined ||
        core.lastPickedCollection === undefined || core.selectionSource === undefined || core.selectionAction !== validated.action ||
        (validated.action === 'toggle-cell' && core.selectionSource !== ORIGINAL_MAIN_GUI_CELL_SOURCE) ||
        (validated.action === 'toggle-basis' && core.selectionSource !== ORIGINAL_MAIN_GUI_BASIS_SOURCE) ||
        (validated.action === 'toggle-frame' && core.selectionSource !== ORIGINAL_MAIN_GUI_FRAME_SOURCE) ||
        (validated.action === 'toggle-grid' && core.selectionSource !== ORIGINAL_MAIN_GUI_GRID_SOURCE) ||
        (validated.action === 'toggle-structure' && core.selectionSource !== ORIGINAL_MAIN_GUI_STRUCTURE_SOURCE) ||
        (validated.action === 'toggle-labels' && core.selectionSource !== ORIGINAL_MAIN_GUI_LABELS_SOURCE) ||
        ((validated.action === 'toggle-hydrogens' || validated.action === 'toggle-hydrogen-bonds') &&
          core.selectionSource !== ORIGINAL_MAIN_GUI_HYDROGEN_SOURCE) ||
        ((validated.action === 'toggle-qpeaks' || validated.action === 'toggle-qpeak-bonds') &&
          core.selectionSource !== ORIGINAL_MAIN_GUI_QPEAK_SOURCE) ||
        ((validated.action === 'fragment-select' || validated.action === 'fragment-select-atoms' ||
          validated.action === 'fragment-select-bonds' || validated.action === 'fragment-hide' ||
          validated.action === 'fragment-show-only') && core.selectionSource !== ORIGINAL_MAIN_GUI_FRAGMENT_SOURCE) ||
        ((validated.action === 'fragment-select' || validated.action === 'fragment-select-atoms' ||
          validated.action === 'fragment-select-bonds') && core.selectedObjectCount === 0) ||
        core.selectionSequence === 0 || (previousSequence !== undefined && core.selectionSequence <= previousSequence)) {
        throw new Error('未收到本次实际选择动作的完整状态；不推测选中结果。');
      }
      let actionLabel: string = validated.action === 'pick' ? (core.pickHit ? '单击命中' : '单击未命中') :
        validated.action === 'clear-selection' ? '已清除选择' : validated.action === 'select-all' ? '已全选' :
        validated.action === 'invert-selection' ? '已反选' : validated.action === 'toggle-cell' ? '已切换晶胞显示' :
          validated.action === 'toggle-basis' ? '已切换基矢显示' : validated.action === 'toggle-frame' ? '已切换包围框显示' :
            validated.action === 'toggle-grid' ? '已切换网格显示' : validated.action === 'center-model' ? '模型已居中' :
              validated.action === 'hide-selection' ? '已隐藏所选图形' : validated.action === 'quality-1' ? '已切换至低绘图质量' :
                validated.action === 'quality-2' ? '已切换至中绘图质量' : validated.action === 'quality-3' ? '已切换至高绘图质量' :
                  validated.action === 'toggle-structure' ? '整体结构显示状态已读回' :
                  validated.action === 'toggle-labels' ? '标签显示状态已读回' :
                  validated.action === 'toggle-hydrogens' ? '氢原子显示状态已读回' :
                    validated.action === 'toggle-hydrogen-bonds' ? '氢键显示状态已读回' :
                      validated.action === 'toggle-qpeaks' ? 'Q 峰显示状态已读回' :
                        validated.action === 'toggle-qpeak-bonds' ? 'Q 峰键显示状态已读回' :
                  validated.action === 'fragment-select' ? '已选中整个片段' :
                  validated.action === 'fragment-select-atoms' ? '已选中片段原子' :
                  validated.action === 'fragment-select-bonds' ? '已选中片段键' :
                  validated.action === 'fragment-hide' ? '已隐藏所选片段' :
                  validated.action === 'fragment-show-only' ? '仅显示所选片段并重新居中' :
                  validated.action === 'view-along-selection' ? '已沿所选键/平面调整视向' : validated.action.startsWith('view-along-') ?
            `视向已切换至 ${validated.action.substring('view-along-'.length)}` : '已显示全部分子并重新居中';
      if (actualVisibility !== undefined) {
        const visibilityLabel: string = validated.action === 'toggle-cell' ? '晶胞' :
          validated.action === 'toggle-basis' ? '基矢' : validated.action === 'toggle-frame' ? '包围框' :
            validated.action === 'toggle-grid' ? '网格' : validated.action === 'toggle-structure' ? '整体结构' :
              validated.action === 'toggle-labels' ? '标签' : validated.action === 'toggle-hydrogens' ? '氢原子' :
                validated.action === 'toggle-hydrogen-bonds' ? '氢键' :
                  validated.action === 'toggle-qpeaks' ? 'Q 峰' : 'Q 峰键';
        actionLabel = `${visibilityLabel}已${actualVisibility ? '显示' : '隐藏'}（原生状态已核对）`;
      }
      this.originalSelectionMessage = `${actionLabel} · 选中 ${core.selectedObjectCount} · 选择序号 ${core.selectionSequence}` +
        (core.lastPickedCollection.length > 0 ? ` · 集合 ${core.lastPickedCollection}` : '');
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版选择操作未完成；可关闭文档。';
    } finally {
      this.originalSelectionBusy = false;
      this.refreshGraphics();
      this.syncOriginalViewportInput();
    }
  }

  private originalPointerMode(ctrl: boolean, shift: boolean): number {
    return originalSelectionMode(this.originalSelectionMode, ctrl, shift);
  }

  private startViewportPointer(kind: string, id: number, xVp: number, yVp: number, primary: boolean): void {
    if (this.originalViewportInput.pointerDown(kind, id, xVp, yVp, primary)) {
      this.dragYaw = this.yaw; this.dragPitch = this.pitch;
    }
  }

  private moveViewportPointer(kind: string, id: number, xVp: number, yVp: number): void {
    const drag: OriginalViewportDrag | undefined = this.originalViewportInput.pointerMove(kind, id, xVp, yVp);
    if (drag === undefined || !this.viewportInputAllowed()) return;
    this.yaw = this.dragYaw + drag.offsetX * 0.35;
    this.pitch = Math.max(-85, Math.min(85, this.dragPitch + drag.offsetY * 0.35));
    this.setView();
  }

  private viewportMouse(event: MouseEvent): void {
    this.syncOriginalViewportInput();
    if (!this.viewportInputAllowed()) { this.originalViewportInput.cancelPointer(); return; }
    if (event.action === MouseAction.Press) {
      if (event.button === MouseButton.Left) this.getUIContext().getFocusController().requestFocus('olex2-viewport-input');
      this.startViewportPointer('mouse', 0, event.x, event.y, event.button === MouseButton.Left);
    } else if (event.action === MouseAction.Move) {
      this.moveViewportPointer('mouse', 0, event.x, event.y);
    } else if (event.action === MouseAction.Release) {
      if (event.button !== MouseButton.Left) { this.originalViewportInput.cancelPointer(); return; }
      const mode: number = this.originalPointerMode(event.getModifierKeyState ? event.getModifierKeyState(['ctrl']) : false,
        event.getModifierKeyState ? event.getModifierKeyState(['shift']) : false);
      this.moveViewportPointer('mouse', 0, event.x, event.y);
      const request: OriginalSelectionRequest | undefined = this.originalViewportInput.pointerUp('mouse', 0, event.x, event.y, mode);
      if (request !== undefined) this.performOriginalSelection(request);
    } else if (event.action === MouseAction.LEAVE_WINDOW || event.action === MouseAction.CANCEL) {
      this.originalViewportInput.cancelPointer();
    }
  }

  private viewportTouch(event: TouchEvent): void {
    // Mouse can also synthesize touch callbacks. Only physical touch enters
    // this path, so a mouse release cannot dispatch a second pick.
    if (event.source !== SourceType.TouchScreen) return;
    this.syncOriginalViewportInput();
    if (!this.viewportInputAllowed() || event.type === TouchType.Cancel || event.touches.length > 1 ||
      event.changedTouches.length !== 1) { this.originalViewportInput.cancelPointer(); return; }
    const point: TouchObject = event.changedTouches[0];
    if (event.type === TouchType.Down) {
      this.getUIContext().getFocusController().requestFocus('olex2-viewport-input');
      this.startViewportPointer('touch', point.id, point.x, point.y, true);
    } else if (event.type === TouchType.Move) {
      this.moveViewportPointer('touch', point.id, point.x, point.y);
    } else if (event.type === TouchType.Up) {
      const mode: number = this.originalPointerMode(event.getModifierKeyState ? event.getModifierKeyState(['ctrl']) : false,
        event.getModifierKeyState ? event.getModifierKeyState(['shift']) : false);
      this.moveViewportPointer('touch', point.id, point.x, point.y);
      const request: OriginalSelectionRequest | undefined = this.originalViewportInput.pointerUp('touch', point.id, point.x, point.y, mode);
      if (request !== undefined) this.performOriginalSelection(request);
    }
  }

  private updateOriginalTextEditorState(): void {
    this.originalTextEditorSource = this.originalTextEditor.getText();
    this.originalTextEditorState = this.originalTextEditor.getStatus();
    const history: OriginalTextHistoryStatus = this.originalTextHistory.getStatus();
    this.originalTextCanUndo = history.canUndo; this.originalTextCanRedo = history.canRedo;
    this.syncOriginalDocumentSessions();
  }

  private syncOriginalDocumentSessions(): void {
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(this.boundOriginalDocumentId);
    if (ui !== undefined) {
      ui.editorVisible = this.originalTextEditorVisible; ui.findVisible = this.originalTextFindVisible;
      ui.query = this.originalTextQuery; ui.replacement = this.originalTextReplacement;
      ui.selectionStart = this.originalTextSelectionStart; ui.selectionEnd = this.originalTextSelectionEnd;
      ui.message = this.originalTextEditorMessage; ui.sourceKind = this.originalDocumentSourceKind;
      ui.toolsAction = this.originalTextToolsAction; ui.toolsSequence = this.originalTextToolsSequence;
      ui.replaceCount = this.originalTextReplaceCount;
      const state: OriginalDocumentEditorStatus = ui.editor.getStatus();
      const summaries: OriginalDocumentSessionSummary[] = this.originalDocumentSessions.getSummaries();
      for (const summary of summaries) {
        if (summary.id === ui.id && (summary.draftDirty !== state.draftDirty || summary.documentDirty !== state.documentDirty)) {
          this.originalDocumentSessions.setDirty(ui.id, state.draftDirty, state.documentDirty);
        }
      }
    }
    this.originalDocumentSessionsStatus = this.originalDocumentSessions.getStatus();
    this.originalDocumentTabs = this.originalDocumentSessions.getSummaries();
    this.originalDocumentSaveTargetSummaries = this.originalDocumentSaveTargets.getSummaries();
    this.scheduleOriginalDocumentRecovery();
  }


  private originalViewportObservationKey(document: OriginalDocumentEnvelope): string {
    return JSON.stringify([document.core.name, document.core.byteCount, document.core.sha256, document.core.revision,
      document.core.selectionSequence, this.graphics.yaw, this.graphics.pitch, this.graphics.distance]);
  }

  private originalViewportSummary(viewport: OriginalDocumentViewportState): OriginalDocumentViewportSummary {
    return { name: viewport.name, byteCount: viewport.byteCount, sha256: viewport.sha256,
      yaw: viewport.yaw, pitch: viewport.pitch, distance: viewport.distance, objectCount: viewport.objectCount,
      inventorySha256: viewport.inventorySha256, selectedObjectCount: viewport.selectedObjectIndices.length,
      selectedIndicesSha256: originalDocumentSha256(encodeOriginalDocumentText(JSON.stringify(viewport.selectedObjectIndices))) };
  }

  private readOriginalViewportForSource(expected: OriginalDocumentEnvelope): OriginalDocumentViewportState {
    const viewport: OriginalDocumentViewportState = parseOriginalDocumentViewport(nativeBridge.captureOriginalDocumentViewport());
    const raw: string = nativeBridge.getOriginalDocumentStatus();
    const observed: OriginalDocumentEnvelope = parseOriginalDocument(raw);
    this.originalDocument = observed; this.originalDocumentRaw = raw;
    this.graphics = nativeBridge.getGraphicsStatus();
    const retained: OriginalDocumentViewportOwnerCore = observed.core as OriginalDocumentViewportOwnerCore;
    if (observed.sessionRetained !== true || retained.contextRetained !== true ||
      !Number.isFinite(this.graphics.yaw) || !Number.isFinite(this.graphics.pitch) || !Number.isFinite(this.graphics.distance) ||
      !this.sameOriginalSource(expected, observed) ||
      !originalDocumentViewportMatchesSource(viewport, observed.core.name, observed.core.byteCount, observed.core.sha256) ||
      observed.core.selectedObjectCount !== viewport.selectedObjectIndices.length ||
      Math.abs(this.graphics.yaw - viewport.yaw) > 0.0001 || Math.abs(this.graphics.pitch - viewport.pitch) > 0.0001 ||
      Math.abs(this.graphics.distance - viewport.distance) > 0.0001) {
      throw new Error('实际视角/选择快照未通过来源、原生选中数量和相机读回核对，未记给任何标签');
    }
    this.yaw = this.graphics.yaw; this.pitch = this.graphics.pitch; this.distance = this.graphics.distance;
    return copyOriginalDocumentViewport(viewport);
  }

  private captureCurrentOriginalViewport(reason: string, force: boolean): void {
    const document: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
    const id: string = this.requireOriginalSessionNative(document);
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (ui === undefined) throw new Error('当前原生文档缺少独立视口会话');
    this.originalDocument = document; this.graphics = nativeBridge.getGraphicsStatus();
    const key: string = this.originalViewportObservationKey(document);
    if (!force && ui.viewport !== undefined && ui.viewportObservationKey === key &&
      ui.viewport.name === document.core.name && ui.viewport.byteCount === document.core.byteCount && ui.viewport.sha256 === document.core.sha256) {
      return;
    }
    const viewport: OriginalDocumentViewportState = this.readOriginalViewportForSource(document);
    ui.viewport = copyOriginalDocumentViewport(viewport); ui.viewportSummary = this.originalViewportSummary(viewport); ui.viewportSequence++;
    ui.viewportObservationKey = this.originalViewportObservationKey(this.originalDocument);
    this.originalDocumentViewportAudit.sequence++; this.originalDocumentViewportAudit.captureCount++;
    this.originalDocumentViewportAudit.documentId = id; this.originalDocumentViewportAudit.action = reason;
    this.originalDocumentViewportAudit.state = 'captured'; this.originalDocumentViewportAudit.error = '';
  }

  private captureOriginalViewportBeforeReplacement(reason: string): void {
    if (this.originalDocumentSessions.getStatus().nativeSessionId.length > 0) {
      this.captureCurrentOriginalViewport(reason, true);
    }
  }

  private observeOriginalDocumentViewport(): void {
    const status: OriginalDocumentSessionsStatus = this.originalDocumentSessions.getStatus();
    if (this.originalDocumentOperationBusy() || this.optionsTransferBusy || this.pythonTransferBusy ||
      status.nativeSessionId.length === 0 || status.nativeSessionId !== this.boundOriginalDocumentId ||
      !this.originalDocumentSessions.matchesNative(this.originalDocument)) return;
    try { this.captureCurrentOriginalViewport('observed-change', false); }
    catch (error) {
      this.originalDocumentViewportAudit.sequence++; this.originalDocumentViewportAudit.documentId = status.nativeSessionId;
      this.originalDocumentViewportAudit.action = 'observed-change'; this.originalDocumentViewportAudit.state = 'failed';
      this.originalDocumentViewportAudit.error = (error as Error).message || '当前视角和选择未确认捕获';
      this.originalSelectionMessage = this.originalDocumentViewportAudit.error + '；上一份有效会话副本保留。';
    }
  }

  private restoreOriginalViewportForSource(id: string, presented: OriginalDocumentEnvelope,
    expected?: OriginalDocumentViewportState): OriginalDocumentViewportState {
    this.originalDocumentViewportAudit.sequence++; this.originalDocumentViewportAudit.documentId = id;
    this.originalDocumentViewportAudit.action = expected === undefined ? 'new-default' : 'restore';
    this.originalDocumentViewportAudit.state = 'pending'; this.originalDocumentViewportAudit.error = '';
    try {
      if (expected === undefined) {
        // Set a new document's default only AFTER its real load, never mutate the previous tab first.
        this.graphics = nativeBridge.setView(32, -22, 4.4);
      } else {
        const checked: OriginalDocumentViewportState = copyOriginalDocumentViewport(expected);
        if (!originalDocumentViewportMatchesSource(checked, presented.core.name, presented.core.byteCount, presented.core.sha256)) {
          throw new Error('目标标签视角和选择属于另一份来源，未提交标签切换');
        }
        const raw: string = nativeBridge.restoreOriginalDocumentViewport(JSON.stringify(checked));
        const restored: OriginalDocumentEnvelope = parseOriginalDocument(raw);
        this.originalDocumentLastOperation = restored; this.originalDocument = restored; this.originalDocumentRaw = raw;
        if (!this.sameOriginalSource(presented, restored)) throw new Error(restored.error || restored.core.error || '视角恢复后原生来源或呈现未确认');
      }
      const viewport: OriginalDocumentViewportState = this.readOriginalViewportForSource(presented);
      if (expected === undefined) {
        if (viewport.selectedObjectIndices.length !== 0 || Math.abs(viewport.yaw - 32) > 0.0001 ||
          Math.abs(viewport.pitch + 22) > 0.0001 || Math.abs(viewport.distance - 4.4) > 0.0001) {
          throw new Error('新载入文档没有实际取得独立默认视角与空选择');
        }
        this.originalDocumentViewportAudit.defaultCount++; this.originalDocumentViewportAudit.state = 'default-captured';
      } else {
        if (!equalOriginalDocumentViewport(expected, viewport)) throw new Error('恢复后的实际原版对象选择或相机与本标签快照不一致');
        this.originalDocumentViewportAudit.restoreCount++; this.originalDocumentViewportAudit.state = 'restored';
      }
      return viewport;
    } catch (error) {
      this.originalDocumentViewportAudit.state = 'failed';
      this.originalDocumentViewportAudit.error = (error as Error).message || '视角和原版选择未确认恢复';
      throw new Error(this.originalDocumentViewportAudit.error);
    }
  }

  private initializeOriginalViewportForNewTab(id: string): void {
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (ui === undefined) throw new Error('新文档独立视口会话不存在');
    const viewport: OriginalDocumentViewportState = this.restoreOriginalViewportForSource(id, this.originalDocument);
    ui.viewport = copyOriginalDocumentViewport(viewport); ui.viewportSummary = this.originalViewportSummary(viewport); ui.viewportSequence++;
    ui.viewportObservationKey = this.originalViewportObservationKey(this.originalDocument);
    this.originalSelectionMessage = '新标签使用独立默认视角与空选择；后续切换保留各标签自己的视角和原版选择。';
  }

  private loadOriginalDocumentRecovery(): void {
    try {
      const journal: OriginalDocumentRecoveryJournal = new OriginalDocumentRecoveryJournal(
        new HarmonyOriginalDocumentRecoveryPort(this.filesDir), originalDocumentSha256);
      this.originalRecoveryJournal = journal;
      const snapshot: OriginalDocumentRecoverySnapshot | undefined = journal.load();
      this.originalRecoveryStatus = journal.getStatus(); this.originalRecoveryLoaded = true;
      this.originalRecoveryAction = 'loaded'; this.originalRecoveryActionSequence++;
      this.originalRecoveryCanRestore = snapshot !== undefined && snapshot.documents.length > 0;
      this.originalRecoveryPending = this.originalRecoveryCanRestore ||
        this.originalRecoveryStatus.state === 'invalid' || this.originalRecoveryStatus.state === 'conflict';
      this.originalRecoveryState = this.originalRecoveryCanRestore ? 'pending' :
        (this.originalRecoveryPending ? 'failed' : 'empty');
      this.originalRecoveryError = this.originalRecoveryStatus.error;
      if (this.originalRecoveryPending) {
        this.originalRecoveryMessage = '尚未自动恢复、改写原文件或覆盖上次备份；请选择恢复，或确认开始新会话。';
      } else this.originalRecoveryCheckpointKey = this.originalDocumentRecoveryChangeKey();
    } catch (error) {
      this.originalRecoveryState = 'unavailable';
      this.originalRecoveryError = (error as Error).message || '应用私有会话备份不可用';
    }
  }

  private originalDocumentRecoveryChangeKey(): string {
    // Cheap change detection: no source/draft copies or hashing on every graphics frame.
    const parts: string[] = [this.originalDocumentSessions.getStatus().activeId,
      JSON.stringify(this.originalDocumentSaveTargets.getSummaries())];
    for (const summary of this.originalDocumentSessions.getSummaries()) {
      const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(summary.id);
      if (ui === undefined) throw new Error('会话备份缺少独立编辑器，未覆盖上一份有效副本');
      parts.push(JSON.stringify(summary), JSON.stringify(ui.editor.getStatus()), JSON.stringify(ui.history.getStatus()),
        String(ui.editorVisible), String(ui.findVisible), ui.query, ui.replacement,
        String(ui.selectionStart), String(ui.selectionEnd), ui.toolsAction, String(ui.toolsSequence), String(ui.replaceCount),
        String(ui.viewportSequence));
    }
    return JSON.stringify(parts);
  }

  private captureOriginalDocumentRecovery(): OriginalDocumentRecoverySnapshot {
    // A failed current capture must not silently checkpoint an older camera/selection as current.
    if (this.originalDocumentSessions.getStatus().nativeSessionId.length > 0) {
      this.captureCurrentOriginalViewport('checkpoint-current', false);
    }
    const documents: OriginalDocumentRecoveryDocument[] = [];
    for (const summary of this.originalDocumentSessions.getSummaries()) {
      const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(summary.id);
      if (ui === undefined) throw new Error('会话备份缺少独立草稿，上一份有效副本未替换');
      const bytes: Uint8Array = this.originalDocumentSessions.getCachedSource(summary.id);
      if (bytes.byteLength !== summary.sourceBytes || originalDocumentSha256(bytes) !== summary.sourceSha256) {
        throw new Error('已应用来源缓存的字节数或 SHA256 不符，未保存恢复副本');
      }
      const sourceText: string = decodeOriginalDocumentText(bytes);
      const editor: OriginalDocumentEditorStatus = ui.editor.getStatus();
      const draft: string = editor.initialized ? ui.editor.getText() : sourceText;
      const document: OriginalDocumentRecoveryDocument = { id: summary.id, name: summary.name,
        sourceKind: summary.sourceKind, sourceText: sourceText, sourceByteCount: bytes.byteLength,
        sourceSha256: summary.sourceSha256, editorInitialized: editor.initialized, draft: draft,
        documentDirty: editor.documentDirty, history: ui.history.exportRecovery(),
        ui: { editorVisible: ui.editorVisible, findVisible: ui.findVisible, query: ui.query, replacement: ui.replacement,
          selectionStart: ui.selectionStart, selectionEnd: ui.selectionEnd, toolsAction: ui.toolsAction,
          toolsSequence: ui.toolsSequence, replaceCount: ui.replaceCount } };
      const target: OriginalDocumentSaveTarget | undefined = this.originalDocumentSaveTargets.get(summary.id);
      if (target !== undefined) document.target = target;
      if (ui.viewport === undefined || !originalDocumentViewportMatchesSource(ui.viewport, summary.name, summary.sourceBytes, summary.sourceSha256)) {
        throw new Error('标签缺少经实际来源核对的视角与原版选择，未覆盖上一份有效副本');
      }
      document.viewport = copyOriginalDocumentViewport(ui.viewport);
      documents.push(document);
    }
    return { schemaVersion: 2, generation: this.originalRecoveryStatus.generation + 1,
      savedAt: new Date().toISOString(), activeId: this.originalDocumentSessions.getStatus().activeId, documents: documents };
  }

  private scheduleOriginalDocumentRecovery(): void {
    if (!this.originalRecoveryLoaded || this.originalRecoveryPending || this.originalRecoveryBusy ||
      this.originalDocumentBusy || this.originalSelectionBusy || this.originalTextGuardBusy ||
      this.optionsTransferBusy || this.pythonTransferBusy) return;
    try {
      const key: string = this.originalDocumentRecoveryChangeKey();
      if (key === this.originalRecoveryCheckpointKey) {
        if (this.originalRecoveryTimer >= 0) clearTimeout(this.originalRecoveryTimer);
        this.originalRecoveryTimer = -1; this.originalRecoveryScheduledKey = key; return;
      }
      if (key === this.originalRecoveryScheduledKey) return;
      this.originalRecoveryScheduledKey = key;
      if (this.originalRecoveryTimer >= 0) clearTimeout(this.originalRecoveryTimer);
      this.originalRecoveryTimer = setTimeout(() => {
        this.originalRecoveryTimer = -1;
        if (this.originalDocumentOperationBusy() || this.optionsTransferBusy || this.pythonTransferBusy) {
          this.originalRecoveryScheduledKey = ''; return;
        }
        this.flushOriginalDocumentRecovery(false);
      }, 250);
    } catch (error) {
      this.originalRecoveryState = 'checkpoint-failed';
      this.originalRecoveryError = (error as Error).message || '无法捕获完整会话，旧副本保留';
    }
  }

  private flushOriginalDocumentRecovery(force: boolean): void {
    const journal: OriginalDocumentRecoveryJournal | undefined = this.originalRecoveryJournal;
    if (journal === undefined || !this.originalRecoveryLoaded || this.originalDocumentOperationBusy() ||
      this.optionsTransferBusy || this.pythonTransferBusy) return;
    if (this.originalRecoveryTimer >= 0) clearTimeout(this.originalRecoveryTimer);
    this.originalRecoveryTimer = -1; this.originalRecoveryBusy = true;
    try {
      this.syncOriginalDocumentSessions();
      if (this.originalDocumentSessions.getStatus().nativeSessionId.length > 0) {
        this.captureCurrentOriginalViewport('checkpoint-current', false);
      }
      const key: string = this.originalDocumentRecoveryChangeKey();
      if (!force && key === this.originalRecoveryCheckpointKey) return;
      this.originalRecoveryScheduledKey = key;
      this.originalRecoveryAction = force ? 'checkpoint-explicit' : 'checkpoint-changed';
      this.originalRecoveryActionSequence++;
      const snapshot: OriginalDocumentRecoverySnapshot = journal.write(this.captureOriginalDocumentRecovery());
      this.originalRecoveryStatus = journal.getStatus();
      this.originalRecoveryCheckpointKey = key;
      this.originalRecoveryState = snapshot.documents.length > 0 ? 'checkpointed' : 'empty';
      this.originalRecoveryError = '';
      this.originalRecoveryMessage = '最近成功副本已回读核对；后续变更约 250 ms 后再备份，不替代原文件保存，也不承诺突发终止零丢失。';
    } catch (error) {
      this.originalRecoveryStatus = journal.getStatus(); this.originalRecoveryState = 'checkpoint-failed';
      this.originalRecoveryError = (error as Error).message || '本次备份未确认完成；上一份有效副本保留';
    } finally { this.originalRecoveryBusy = false; this.saveRecord(); }
  }

  private loadOriginalRecoveryNative(name: string, bytes: Uint8Array, sha256: string): OriginalDocumentRecoveryPresented {
    // Only the persisted applied SOURCE is loaded. Drafts, history and saved output are never native inputs here.
    this.originalRecoveryTemporaryNative = true;
    const raw: string = nativeBridge.openOriginalDocument(name, bytes);
    const opened: OriginalDocumentEnvelope = parseOriginalDocument(raw);
    this.originalDocumentLastOperation = opened; this.originalDocument = opened; this.originalDocumentRaw = raw;
    const actual: Uint8Array = nativeBridge.readOriginalDocumentSource();
    const statusRaw: string = nativeBridge.getOriginalDocumentStatus();
    const observed: OriginalDocumentEnvelope = parseOriginalDocument(statusRaw);
    this.originalDocument = observed; this.originalDocumentRaw = statusRaw;
    if (!this.sameOriginalSource(opened, observed) || observed.core.name !== name ||
      observed.core.sha256 !== sha256 || actual.byteLength !== bytes.byteLength || originalDocumentSha256(actual) !== sha256) {
      throw new Error('真实重新载入的来源未通过名称、字节数、SHA256 和呈现核对；未提交恢复会话');
    }
    return { document: observed, bytes: actual };
  }

  private closeOriginalRecoveryTemporaryNative(): void {
    if (!this.originalRecoveryTemporaryNative) return;
    const raw: string = nativeBridge.closeOriginalDocument();
    const closed: OriginalDocumentEnvelope = parseOriginalDocument(raw);
    this.originalDocumentLastOperation = closed; this.originalDocument = closed; this.originalDocumentRaw = raw;
    if (!closed.passed || closed.core.loaded || closed.sessionRetained === true || closed.core.modelMayHaveChanged) {
      throw new Error('恢复临时原生文档未确认关闭；不声称空会话，也未覆盖上次备份');
    }
    this.originalRecoveryTemporaryNative = false;
  }

  private originalRecoverySurfaceReady(): boolean {
    return this.graphics.surfaceReady && this.graphics.initialized &&
      Number.isFinite(this.graphics.width) && Number.isFinite(this.graphics.height) &&
      this.graphics.width > 0 && this.graphics.height > 0;
  }

  private restoreOriginalDocumentRecovery(): void {
    const journal: OriginalDocumentRecoveryJournal | undefined = this.originalRecoveryJournal;
    if (journal === undefined || !this.originalRecoveryPending || !this.originalRecoveryCanRestore ||
      this.originalRecoveryBusy || this.originalDocumentBusy || this.originalSelectionBusy || this.originalTextGuardBusy ||
      this.optionsTransferBusy || this.pythonTransferBusy) return;
    // No Open/Close/read, temporary-owner flag, or checkpoint write may run
    // until the actual main surface has a finite, nonzero measured size.
    this.graphics = nativeBridge.getGraphicsStatus();
    if (!this.originalRecoverySurfaceReady()) {
      this.originalRecoveryMessage = '绘图窗口正在准备；就绪后请明确点击恢复。上次会话副本保持不变。';
      this.saveRecord(); return;
    }
    this.originalRecoveryBusy = true; this.originalRecoveryAction = 'restore'; this.originalRecoveryActionSequence++;
    try {
      if (this.originalDocumentSessions.getStatus().count !== 0) throw new Error('已有当前文档，不能用恢复会话替换');
      this.closeOriginalRecoveryTemporaryNative();
      this.graphics = nativeBridge.getGraphicsStatus();
      if (this.originalDocumentActive()) throw new Error('原生文档不是空会话；未覆盖当前文档或上次备份');
      const snapshot: OriginalDocumentRecoverySnapshot | undefined = journal.getSnapshot();
      if (snapshot === undefined || snapshot.documents.length === 0) throw new Error('没有可恢复的完整会话');
      const sessions: OriginalDocumentSessions = new OriginalDocumentSessions();
      const editors: Map<string, OriginalDocumentSessionUi> = new Map<string, OriginalDocumentSessionUi>();
      const targets: OriginalDocumentSaveTargets = new OriginalDocumentSaveTargets();
      for (const document of snapshot.documents) {
        if (!document.editorInitialized && (document.documentDirty || document.ui.editorVisible)) {
          throw new Error('未初始化编辑器的脏状态或可见状态不一致；未提交恢复会话');
        }
        const presented: OriginalDocumentRecoveryPresented = this.loadOriginalRecoveryNative(document.name,
          encodeOriginalDocumentText(document.sourceText), document.sourceSha256);
        const viewport: OriginalDocumentViewportState = this.restoreOriginalViewportForSource(document.id, presented.document, document.viewport);
        presented.document = this.originalDocument;
        const editor: OriginalDocumentEditor = new OriginalDocumentEditor();
        if (document.editorInitialized) editor.restoreRecovered(presented.document, presented.bytes, document.draft, document.documentDirty);
        const history: OriginalTextHistory = new OriginalTextHistory(); history.restoreRecovery(document.history);
        const state: OriginalDocumentEditorStatus = editor.getStatus();
        sessions.restorePresented(document.id, presented.document, presented.bytes, document.sourceKind, state.draftDirty, state.documentDirty);
        const ui: OriginalDocumentSessionUi = { id: document.id, editor: editor, history: history,
          editorVisible: document.ui.editorVisible, findVisible: document.ui.findVisible, query: document.ui.query,
          replacement: document.ui.replacement, selectionStart: document.ui.selectionStart, selectionEnd: document.ui.selectionEnd,
          message: '已恢复草稿和撤销历史；未自动应用草稿或保存到原文件。', sourceKind: document.sourceKind,
          toolsAction: document.ui.toolsAction, toolsSequence: document.ui.toolsSequence, replaceCount: document.ui.replaceCount,
          viewport: copyOriginalDocumentViewport(viewport), viewportSummary: this.originalViewportSummary(viewport),
          viewportSequence: 1, viewportObservationKey: '' };
        editors.set(document.id, ui);
        if (document.target !== undefined) targets.restoreRemembered(document.target);
      }
      if (sessions.getStatus().nativeSessionId !== snapshot.activeId) {
        const request: OriginalDocumentActivation = sessions.beginActivation(snapshot.activeId);
        const active: OriginalDocumentSessionUi | undefined = editors.get(snapshot.activeId);
        if (active === undefined) throw new Error('恢复活动标签不存在');
        const presented: OriginalDocumentRecoveryPresented = this.loadOriginalRecoveryNative(request.name, request.bytes,
          originalDocumentSha256(request.bytes));
        const viewport: OriginalDocumentViewportState = this.restoreOriginalViewportForSource(request.id, presented.document, active.viewport);
        presented.document = this.originalDocument; active.viewport = copyOriginalDocumentViewport(viewport);
        active.viewportSummary = this.originalViewportSummary(viewport); active.viewportSequence++;
        if (active.editor.getStatus().initialized) active.editor.reactivate(presented.document, presented.bytes);
        sessions.acceptActivation(request, presented.document, presented.bytes);
      }
      // Commit page ownership only AFTER all real native loads and final active binding succeed.
      this.originalDocumentSessions = sessions; this.originalDocumentSessionUi = editors;
      this.originalDocumentSaveTargets = targets; this.originalRecoveryPending = false;
      this.originalRecoveryCanRestore = false; this.originalRecoveryTemporaryNative = false;
      this.pythonEditorVisible = false;
      const active: OriginalDocumentSessionUi | undefined = editors.get(snapshot.activeId);
      if (active === undefined) throw new Error('恢复活动标签的视口会话不存在');
      active.viewportObservationKey = this.originalViewportObservationKey(this.originalDocument);
      this.bindOriginalDocumentUi(snapshot.activeId);
      this.originalRecoveryRestoredCount = snapshot.documents.length;
      this.originalRecoveryState = 'restored'; this.originalRecoveryError = '';
      this.originalRecoveryMessage = '已真实重新载入已应用来源，并逐份恢复视角与原版选择、草稿、撤销/重做和保存位置元数据；没有自动应用或外部保存。';
      // Legacy slots contain no viewport: real default captures are newly checkpointed, never attributed to old evidence.
      const legacyViewport: boolean = snapshot.documents.some((document: OriginalDocumentRecoveryDocument) => document.viewport === undefined);
      this.originalRecoveryCheckpointKey = legacyViewport ? '' : this.originalDocumentRecoveryChangeKey();
      this.originalRecoveryScheduledKey = this.originalRecoveryCheckpointKey;
      this.originalDocumentMessage = '上次会话已恢复；保存时仍需核对原位置旧内容及当前访问权限。';
    } catch (error) {
      this.originalRecoveryState = 'restore-failed';
      this.originalRecoveryError = (error as Error).message || '本次恢复未完成；上次备份保留';
      try { this.closeOriginalRecoveryTemporaryNative(); }
      catch (closeError) { this.originalRecoveryError += '；' + (closeError as Error).message; }
      if (!this.originalRecoveryTemporaryNative && this.originalDocumentSessions.getStatus().count === 0) this.detachOriginalDocumentUi();
    } finally {
      this.originalRecoveryBusy = false; this.syncOriginalDocumentSessions(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private confirmDiscardOriginalDocumentRecovery(): void {
    if (!this.originalRecoveryPending || this.originalRecoveryBusy) return;
    this.originalRecoveryBusy = true; this.originalRecoveryAction = 'discard-confirmation'; this.originalRecoveryActionSequence++;
    this.saveRecord();
    AlertDialog.show({ title: '不恢复上次会话？',
      message: '确认后写入一个新的空会话副本，上次会话将不再自动提示。不会删除或改写原文件；旧备份槽不代表安全擦除。',
      primaryButton: { value: '取消，保留上次会话', action: () => {
        this.originalRecoveryBusy = false; this.originalRecoveryAction = 'discard-cancelled'; this.saveRecord();
      } },
      secondaryButton: { value: '确认开始新会话', action: () => this.discardOriginalDocumentRecovery() },
      cancel: () => { this.originalRecoveryBusy = false; this.originalRecoveryAction = 'discard-dismissed'; this.saveRecord(); } });
  }

  private discardOriginalDocumentRecovery(): void {
    const journal: OriginalDocumentRecoveryJournal | undefined = this.originalRecoveryJournal;
    if (journal === undefined || !this.originalRecoveryPending) { this.originalRecoveryBusy = false; return; }
    this.originalRecoveryAction = 'discard-confirmed'; this.originalRecoveryActionSequence++;
    try {
      if (this.originalDocumentSessions.getStatus().count !== 0) throw new Error('当前文档仍存在，不能写成空会话');
      this.closeOriginalRecoveryTemporaryNative();
      this.graphics = nativeBridge.getGraphicsStatus();
      if (this.originalDocumentActive()) throw new Error('当前原生文档未确认关闭，旧恢复副本保留');
      journal.startEmptyConfirmed({ schemaVersion: 2, generation: this.originalRecoveryStatus.generation + 1,
        savedAt: new Date().toISOString(), activeId: '', documents: [] });
      this.originalRecoveryStatus = journal.getStatus(); this.originalRecoveryPending = false;
      this.originalRecoveryCanRestore = false; this.originalRecoveryState = 'empty'; this.originalRecoveryError = '';
      this.originalRecoveryMessage = '已确认开始新会话，并回读核对新的空副本；原文件没有改写。';
      this.originalRecoveryCheckpointKey = this.originalDocumentRecoveryChangeKey();
      this.originalRecoveryScheduledKey = this.originalRecoveryCheckpointKey;
    } catch (error) {
      this.originalRecoveryStatus = journal.getStatus(); this.originalRecoveryState = 'write-failed';
      this.originalRecoveryError = (error as Error).message || '空会话未确认写入，仍保留上次会话';
    } finally { this.originalRecoveryBusy = false; this.saveRecord(); this.syncOriginalViewportInput(); }
  }

  private originalDocumentSavedName(): string {
    const target: OriginalDocumentSaveTarget | undefined = this.originalDocumentSaveTargets.get(this.boundOriginalDocumentId);
    return target === undefined ? '' : target.name;
  }

  private originalDocumentSavedByteCount(): number {
    const target: OriginalDocumentSaveTarget | undefined = this.originalDocumentSaveTargets.get(this.boundOriginalDocumentId);
    return target === undefined ? 0 : target.byteCount;
  }

  private originalDocumentSessionEditorAudit(): OriginalDocumentSessionEditorAudit[] {
    const result: OriginalDocumentSessionEditorAudit[] = [];
    for (const summary of this.originalDocumentTabs) {
      const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(summary.id);
      if (ui !== undefined) result.push({ id: ui.id, editor: ui.editor.getStatus(), history: ui.history.getStatus(),
        editorVisible: ui.editorVisible, findVisible: ui.findVisible, queryCharacters: ui.query.length,
        replacementCharacters: ui.replacement.length, selectionStart: ui.selectionStart, selectionEnd: ui.selectionEnd,
        viewportSequence: ui.viewportSequence, viewport: ui.viewportSummary });
    }
    return result;
  }

  private beginOriginalSessionOperation(operation: string, id: string): void {
    this.originalDocumentSessionOperation = { sequence: this.originalDocumentSessionOperation.sequence + 1,
      operation: operation, targetId: id, state: 'pending', error: '' };
  }

  private failOriginalSessionOperation(error: Error): void {
    this.originalDocumentSessionOperation.state = 'failed';
    this.originalDocumentSessionOperation.error = error.message || '文档会话操作未确认完成';
    this.originalDocumentMessage = this.originalDocumentSessionOperation.error;
  }

  private bindOriginalDocumentUi(id: string): void {
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (ui === undefined) throw new Error('文档的独立编辑会话不存在，不能沿用另一份草稿');
    this.boundOriginalDocumentId = id; this.originalTextEditor = ui.editor; this.originalTextHistory = ui.history;
    this.originalTextEditorVisible = ui.editorVisible; this.originalTextFindVisible = ui.findVisible;
    this.originalTextQuery = ui.query; this.originalTextReplacement = ui.replacement;
    this.originalTextSelectionStart = ui.selectionStart; this.originalTextSelectionEnd = ui.selectionEnd;
    this.originalTextEditorMessage = ui.message; this.originalDocumentSourceKind = ui.sourceKind;
    this.originalTextToolsAction = ui.toolsAction; this.originalTextToolsSequence = ui.toolsSequence;
    this.originalTextReplaceCount = ui.replaceCount; this.originalTextFocusedControl = '';
    this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = '';
    this.originalTextFocusTarget = ''; this.originalTextFocusSequence++; this.originalTextSelectionSequence++;
    this.originalDocumentShortcutLatch.reset(); this.desktopMetaKeys.clear();
    this.originalViewportInput.cancelPointer(); this.originalViewportInput.setFocused(false);
    this.originalViewportFocused = false;
    this.originalSelectionMessage = ui.viewport === undefined ? '新标签视角和原版选择尚未确认捕获。' :
      `本标签视角与原版选择已按实际来源核对 · 选中 ${ui.viewport.selectedObjectIndices.length}；各文档草稿与撤销历史保持独立。`;
    this.updateOriginalTextEditorState();
  }

  private detachOriginalDocumentUi(): void {
    this.boundOriginalDocumentId = ''; this.originalTextEditor = new OriginalDocumentEditor();
    this.originalTextHistory = new OriginalTextHistory(); this.originalTextEditorVisible = false;
    this.originalTextEditorMessage = '实际源文本；另存为经原版保存器规范化。';
    this.originalDocumentSourceKind = ''; this.resetOriginalTextTools(); this.updateOriginalTextEditorState();
    this.originalViewportInput.cancelPointer(); this.originalViewportInput.setFocused(false);
    this.originalViewportFocused = false;
  }

  private requireOriginalSessionNative(document: OriginalDocumentEnvelope): string {
    const status: OriginalDocumentSessionsStatus = this.originalDocumentSessions.getStatus();
    if (status.activeId.length === 0 || status.activeId !== this.boundOriginalDocumentId ||
      !this.originalDocumentSessions.matchesNative(document)) {
      throw new Error('当前标签与原生文档尚未确认绑定；请点击目标标签重新载入，禁止用另一份草稿应用或保存');
    }
    return status.activeId;
  }

  private sameOriginalSource(left: OriginalDocumentEnvelope, right: OriginalDocumentEnvelope): boolean {
    return originalDocumentPresented(left) && originalDocumentPresented(right) &&
      left.core.name === right.core.name && left.core.revision === right.core.revision &&
      left.core.sha256 === right.core.sha256 && left.core.byteCount === right.core.byteCount;
  }

  private reconcileOriginalSessionNative(): void {
    // A restored/refreshed session can keep the same stable source (name, format,
    // bytes, revision, source) while volatile presentation fields (selection,
    // render inventories) drift. Refresh the presented identity in that case so
    // the document toolbar is not disabled; genuinely changed sources still fail
    // the stable check and are invalidated below.
    if (this.originalDocumentStatusError.length === 0 &&
      this.originalDocumentSessions.getStatus().nativeSessionId.length > 0 &&
      !this.originalDocumentSessions.matchesNative(this.originalDocument) &&
      (this.originalDocumentSessions.matchesNativeSourceIdentity(this.originalDocument) ||
        this.originalDocumentSessions.matchesNativeSourceRevision(this.originalDocument)) &&
      originalDocumentPresented(this.originalDocument)) {
      this.originalDocumentSessions.refreshPresentedIdentity(this.originalDocument);
    }
    if (this.originalDocumentSessions.getStatus().nativeSessionId.length > 0 &&
      (this.originalDocumentStatusError.length > 0 ||
        (!this.originalDocumentSessions.matchesNative(this.originalDocument) &&
          !this.originalDocumentSessions.matchesNativeSourceIdentity(this.originalDocument)))) {
      this.originalDocumentSessions.invalidateNative();
    }
    this.syncOriginalDocumentSessions();
  }

  private performOriginalDocumentActivation(id: string): void {
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (ui === undefined) throw new Error('目标文档的独立草稿不存在');
    this.captureOriginalViewportBeforeReplacement('before-tab-switch');
    const request: OriginalDocumentActivation = this.originalDocumentSessions.beginActivation(id);
    let observed: OriginalDocumentEnvelope | undefined;
    try {
      const raw: string = nativeBridge.openOriginalDocument(request.name, request.bytes);
      const opened: OriginalDocumentEnvelope = parseOriginalDocument(raw);
      this.originalDocumentLastOperation = opened; this.originalDocument = opened; this.originalDocumentRaw = raw;
      const bytes: Uint8Array = nativeBridge.readOriginalDocumentSource();
      observed = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
      this.originalDocument = observed;
      if (!this.sameOriginalSource(opened, observed)) throw new Error('重载来源后原生文档身份改变，不能确认标签激活');
      // Real source load and actual viewport readback BOTH precede committing target ownership.
      const viewport: OriginalDocumentViewportState = this.restoreOriginalViewportForSource(id, observed, ui.viewport);
      observed = this.originalDocument; ui.viewport = copyOriginalDocumentViewport(viewport);
      ui.viewportSummary = this.originalViewportSummary(viewport); ui.viewportSequence++;
      ui.viewportObservationKey = this.originalViewportObservationKey(observed);
      // Rebind only the unchanged applied baseline; pending draft/history are never sent to native.
      if (ui.editor.getStatus().initialized) ui.editor.reactivate(observed, bytes);
      this.originalDocumentSessions.acceptActivation(request, observed, bytes);
      this.bindOriginalDocumentUi(id);
      this.originalDocumentMessage = `已切换到 ${request.name}；本标签视角、原版选择、草稿、撤销和查找状态已核对保留，未自动应用或保存。`;
      this.originalDocumentSessionOperation.state = 'passed';
    } catch (error) {
      try { observed = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus()); this.originalDocument = observed; }
      catch (statusError) { this.originalDocumentStatusError = (statusError as Error).message || '原生状态无法确认'; }
      if (this.originalDocumentSessions.getStatus().pendingId === id) {
        this.originalDocumentSessions.abortActivation(request, observed);
      } else this.originalDocumentSessions.invalidateNative();
      throw new Error((error as Error).message || '目标文档重新载入未确认成功；各文档草稿仍保留');
    }
  }

  private activateOriginalDocumentSession(id: string): void {
    if (this.originalDocumentOperationBusy() || this.pythonEditorVisible) return;
    const status: OriginalDocumentSessionsStatus = this.originalDocumentSessions.getStatus();
    if (status.activeId === id && this.originalDocumentSessions.matchesNative(this.originalDocument)) return;
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('activate', id);
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try { this.performOriginalDocumentActivation(id); }
    catch (error) { this.failOriginalSessionOperation(error as Error); }
    finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async closeOriginalDocumentSession(id: string, discardConfirmed: boolean = false): Promise<void> {
    if (this.originalDocumentOperationBusy() || this.pythonEditorVisible) return;
    this.syncOriginalDocumentSessions();
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (id.length > 0 && ui === undefined) return;
    if (!discardConfirmed && !this.confirmOriginalTextReplacement(id, () => { this.closeOriginalDocumentSession(id, true); })) return;
    this.beginOriginalSessionOperation('close', id);
    const status: OriginalDocumentSessionsStatus = this.originalDocumentSessions.getStatus();
    if (id.length > 0 && id !== status.activeId && id !== status.nativeSessionId) {
      // Inactive tab close must never call native Close or touch the active editor/history.
      this.originalDocumentSessions.remove(id); this.originalDocumentSessionUi.delete(id);
      this.originalDocumentSaveTargets.retire(id);
      this.originalDocumentSessionOperation.state = 'passed';
      this.originalDocumentMessage = `已关闭非当前标签 ${id}；当前原生文档和草稿未改变。`;
      this.syncOriginalDocumentSessions(); this.saveRecord(); return;
    }
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.run('close',
        this.originalDocumentBackend(), new HarmonyOriginalDocumentPort(context));
      this.acceptOriginalDocumentResult(result);
      if (result.status.state !== 'passed') throw new Error(result.status.error || '原生文档未确认关闭，标签和草稿保留');
      let nextId: string = '';
      if (id.length > 0) {
        nextId = this.originalDocumentSessions.remove(id); this.originalDocumentSessionUi.delete(id);
        this.originalDocumentSaveTargets.retire(id);
      }
      this.detachOriginalDocumentUi();
      this.originalDocumentSessionOperation.state = 'passed';
      if (nextId.length > 0) this.performOriginalDocumentActivation(nextId);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private openOriginalTextEditor(): void {
    if (this.originalDocumentOperationBusy() || !this.originalDocumentViewPresented()) return;
    try {
      if (!this.originalTextEditor.getStatus().initialized) {
        const bytes: Uint8Array = nativeBridge.readOriginalDocumentSource();
        const document: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
        this.requireOriginalSessionNative(document);
        this.originalTextEditor.capture(document, bytes);
        this.originalTextHistory.reset(this.originalTextEditor.getText());
        this.originalTextEditorMessage = `已读取实际源文件 ${bytes.byteLength} 字节；修改不会自动应用或保存。`;
      }
      this.updateOriginalTextEditorState();
      this.originalViewportInput.setFocused(false); this.originalViewportFocused = false;
      this.desktopShortcutLatch.reset(); this.desktopMetaKeys.clear();
      this.pythonEditorVisible = false; this.originalTextEditorVisible = true;
      this.syncOriginalViewportInput();
    } catch (error) {
      this.originalDocumentMessage = (error as Error).message || '实际源文本无法读取，当前文档未改变。';
    }
    this.saveRecord();
  }

  private editOriginalText(text: string): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    this.originalTextEditor.edit(text); this.originalTextHistory.record(text);
    this.updateOriginalTextEditorState();
    this.originalTextToolsAction = 'edit'; this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    this.originalTextEditorMessage = '草稿尚未应用；返回视图保留草稿，应用后仍需另存为。';
  }

  private resetOriginalTextTools(): void {
    this.originalTextHistory.reset(''); this.originalTextFindVisible = false;
    this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = '';
    this.originalTextQuery = ''; this.originalTextReplacement = '';
    this.originalTextSelectionStart = 0; this.originalTextSelectionEnd = 0;
    this.originalTextFocusedControl = ''; this.originalTextFocusTarget = '';
    this.originalTextFocusSequence++; this.originalTextSelectionSequence++;
    this.originalTextToolsAction = 'document-reset'; this.originalTextToolsSequence++;
    this.originalTextReplaceCount = 0;
  }

  private focusOriginalTextControl(target: string): void {
    this.originalTextFocusTarget = target; this.originalTextFocusSequence++;
  }

  private selectOriginalText(start: number, end: number): void {
    const length: number = this.originalTextEditorSource.length;
    this.originalTextSelectionStart = Math.min(length, Math.max(0, start));
    this.originalTextSelectionEnd = Math.min(length, Math.max(this.originalTextSelectionStart, end));
    this.originalTextSelectionSequence++;
  }

  private originalTextHistoryAction(action: string): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    const text: string | undefined = action === 'undo' ? this.originalTextHistory.undo() : this.originalTextHistory.redo();
    if (text === undefined) return;
    this.originalTextEditor.edit(text); this.updateOriginalTextEditorState();
    this.originalTextToolsAction = action; this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    this.selectOriginalText(this.originalTextSelectionStart, this.originalTextSelectionStart);
    this.originalTextEditorMessage = action === 'undo' ? '已撤销一笔文本修改；没有自动应用或保存。' :
      '已重做一笔文本修改；没有自动应用或保存。';
    this.saveRecord();
  }

  private showOriginalTextFind(visible: boolean): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    this.originalTextFindVisible = visible;
    if (visible) { this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = ''; }
    this.focusOriginalTextControl(visible ? 'olex2-document-editor-find-query' : 'olex2-document-editor-source');
    this.originalTextToolsAction = visible ? 'find-open' : 'find-close'; this.originalTextToolsSequence++;
    this.saveRecord();
  }

  private findNextOriginalText(): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    if (this.originalTextQuery.length === 0) { this.originalTextEditorMessage = '请先输入要查找的原文。'; return; }
    const match: LiteralTextMatch | undefined = findLiteralText(this.originalTextEditorSource,
      this.originalTextQuery, this.originalTextSelectionEnd);
    this.originalTextToolsAction = 'find-next'; this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    if (match === undefined) this.originalTextEditorMessage = '没有找到原文；文本未改变。';
    else {
      this.selectOriginalText(match.start, match.end);
      this.originalTextEditorMessage = `${match.wrapped ? '已从文首继续，' : ''}找到第 ${match.start + 1} 个字符起的原文；区分大小写，不执行正则表达式。`;
    }
    this.saveRecord();
  }

  private findPreviousOriginalText(): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    if (this.originalTextQuery.length === 0) { this.originalTextEditorMessage = '请先输入要查找的原文。'; return; }
    const match: LiteralTextMatch | undefined = findPreviousLiteralText(this.originalTextEditorSource,
      this.originalTextQuery, this.originalTextSelectionStart);
    this.originalTextToolsAction = 'find-previous'; this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    if (match === undefined) this.originalTextEditorMessage = '没有找到原文；文本未改变。';
    else {
      this.selectOriginalText(match.start, match.end);
      this.originalTextEditorMessage = `${match.wrapped ? '已从文末继续，' : ''}找到第 ${match.start + 1} 个字符起的原文；区分大小写，不执行正则表达式。`;
    }
    this.saveRecord();
  }

  private showOriginalTextGoToLine(visible: boolean): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized) return;
    if (visible) {
      if (!this.originalTextGoToLineVisible) this.originalTextGoToLineInput = '';
      this.originalTextFindVisible = false;
    }
    this.originalTextGoToLineVisible = visible;
    if (!visible) this.originalTextGoToLineInput = '';
    this.focusOriginalTextControl(visible ? 'olex2-document-editor-go-to-line-input' : 'olex2-document-editor-source');
    this.originalTextToolsAction = visible ? 'go-to-line-open' : 'go-to-line-close'; this.originalTextToolsSequence++;
    this.saveRecord();
  }

  private goToOriginalTextLine(): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized ||
      !this.originalTextEditorVisible || !this.originalTextGoToLineVisible) return;
    const target: LiteralTextLineTarget = literalTextLineTarget(this.originalTextEditorSource, this.originalTextGoToLineInput);
    this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    if (target.line === 0 || target.start < 0) {
      this.originalTextToolsAction = 'go-to-line-invalid';
      this.originalTextEditorMessage = target.line === 0 ? `请输入 1 到 ${target.lineCount} 之间的整数行号；文本未改变。` :
        `文档共 ${target.lineCount} 行，没有第 ${target.line} 行；文本未改变。`;
      this.focusOriginalTextControl('olex2-document-editor-go-to-line-input');
    } else {
      this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = '';
      this.focusOriginalTextControl('olex2-document-editor-source');
      this.selectOriginalText(target.start, target.start);
      this.originalTextToolsAction = 'go-to-line';
      this.originalTextEditorMessage = `已定位到第 ${target.line} 行行首（共 ${target.lineCount} 行）；只移动光标，没有应用或保存修改。`;
    }
    this.saveRecord();
  }

  private replaceOriginalText(all: boolean): void {
    if (this.originalDocumentOperationBusy() || !this.originalTextEditorState.initialized || this.originalTextQuery.length === 0) return;
    try {
      if (!all && this.originalTextEditorSource.length - this.originalTextQuery.length +
        this.originalTextReplacement.length > 8 * 1024 * 1024) {
        throw new Error('替换结果超过文本区上限；草稿未改变。');
      }
      const selection: LiteralTextMatch = { start: this.originalTextSelectionStart, end: this.originalTextSelectionEnd, wrapped: false };
      const result: LiteralTextReplacement | undefined = all ?
        replaceAllLiteralText(this.originalTextEditorSource, this.originalTextQuery, this.originalTextReplacement) :
        replaceLiteralMatch(this.originalTextEditorSource, this.originalTextQuery, this.originalTextReplacement, selection);
      if (result === undefined) {
        this.findNextOriginalText();
        this.originalTextEditorMessage += ' 先定位原文，再点击“替换当前”；没有替换未选中的文本。';
        this.saveRecord(); return;
      }
      if (result.replacements > 0) {
        this.editOriginalText(result.text); this.selectOriginalText(result.start, result.end);
      }
      this.originalTextReplaceCount = result.replacements;
      this.originalTextToolsAction = all ? 'replace-all' : 'replace-current'; this.originalTextToolsSequence++;
      this.originalTextEditorMessage = `已按原文替换 ${result.replacements} 处；没有自动应用或保存，可撤销。`;
    } catch (error) {
      this.originalTextEditorMessage = (error as Error).message || '替换未完成，草稿未改变。';
    }
    this.saveRecord();
  }

  private revertOriginalText(): void {
    if (this.originalDocumentOperationBusy()) return;
    this.originalTextEditor.revertDraft(); this.originalTextHistory.record(this.originalTextEditor.getText());
    this.updateOriginalTextEditorState();
    this.originalTextToolsAction = 'revert-draft'; this.originalTextToolsSequence++; this.originalTextReplaceCount = 0;
    this.originalTextEditorMessage = this.originalTextEditorState.documentDirty ?
      '未应用草稿已撤回；已应用到原文档的修改仍需保存。' : '未应用草稿已撤回，原文档未改变。';
    this.saveRecord();
  }

  private applyOriginalText(): void {
    if (this.originalDocumentOperationBusy()) return;
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('apply', this.boundOriginalDocumentId);
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const current: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
      const id: string = this.requireOriginalSessionNative(current);
      const bytes: Uint8Array = this.originalTextEditor.prepareApply(current);
      const raw: string = nativeBridge.openOriginalDocument(this.originalTextEditorState.name, bytes);
      const opened: OriginalDocumentEnvelope = parseOriginalDocument(raw);
      this.originalDocumentLastOperation = opened; this.originalDocument = opened; this.originalDocumentRaw = raw;
      const actualBytes: Uint8Array = nativeBridge.readOriginalDocumentSource();
      const document: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
      this.originalDocument = document;
      if (!this.sameOriginalSource(opened, document)) throw new Error('应用后实际来源的原生文档身份不一致');
      this.originalTextEditor.acceptApplied(document, actualBytes);
      this.originalDocumentSessions.updateApplied(id, document, actualBytes);
      // Applying a changed source retires its old object inventory; capture the actual new empty selection instead.
      this.captureCurrentOriginalViewport('source-applied-new-inventory', true);
      this.originalDocumentSourceKind = this.originalDocumentSourceKind === 'synthetic-platform-display' ||
        this.originalDocumentSourceKind === 'edited-synthetic-platform-display' ? 'edited-synthetic-platform-display' : 'edited-original-source';
      this.originalViewportInput.cancelPointer(); this.originalViewportInput.setFocused(false);
      this.originalViewportFocused = false;
      this.originalSelectionMessage = '文本已由原版载入器重新载入；选择已重置。';
      this.originalTextEditorMessage = '修改已由原版载入器应用到当前文档，尚未保存；另存为时格式可能归一化。';
      this.originalDocumentMessage = this.originalTextEditorMessage;
      this.originalDocumentSessionOperation.state = 'passed';
      this.originalTextApplyRebindPending = true;
    } catch (error) {
      this.failOriginalSessionOperation(error as Error);
      this.originalTextEditorMessage = (error as Error).message || '修改未确认应用；草稿保留。';
      this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
      if (this.originalDocument.core.modelMayHaveChanged) {
        this.originalTextEditorMessage += ' 原生核心报告状态可能改变，不能宣称已回滚。';
      }
    } finally {
      this.updateOriginalTextEditorState(); this.originalDocumentBusy = false;
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
      if (this.originalTextApplyRebindPending) {
        // The native revision advanced after the apply; rebind the presented
        // identity so the document toolbar is not left disabled.
        this.originalTextApplyRebindPending = false;
        this.rebindPresentedSessionAfterModelChange();
        this.readOriginalDocumentStatus();
      }
    }
  }

  private confirmOriginalTextReplacement(id: string, action: () => void): boolean {
    const ui: OriginalDocumentSessionUi | undefined = this.originalDocumentSessionUi.get(id);
    if (ui === undefined || !ui.editor.getStatus().needsSave) return true;
    const editor: OriginalDocumentEditorStatus = ui.editor.getStatus();
    this.originalTextGuardBusy = true; this.originalTextGuardDecision = 'pending'; this.originalTextGuardTargetId = id;
    this.originalTextGuardSequence++; this.syncOriginalViewportInput(); this.saveRecord();
    AlertDialog.show({ title: '关闭文档前确认',
      message: `“${editor.name}”（${id}）有${editor.draftDirty ? '尚未应用的草稿' : '已应用但尚未保存的修改'}。继续只放弃这份文档的修改；其他标签不会关闭。`,
      primaryButton: { value: '取消，保留修改', action: () => {
        this.originalTextGuardBusy = false; this.originalTextGuardDecision = 'cancel';
        this.syncOriginalViewportInput(); this.saveRecord();
      } },
      secondaryButton: { value: '放弃修改并关闭此文档', action: () => {
        this.originalTextGuardBusy = false; this.originalTextGuardDecision = 'discard-confirmed';
        this.syncOriginalViewportInput(); this.saveRecord(); action();
      } },
      cancel: () => {
        this.originalTextGuardBusy = false; this.originalTextGuardDecision = 'dismissed';
        this.syncOriginalViewportInput(); this.saveRecord();
      } });
    return false;
  }

  private originalDocumentBackend(): OriginalDocumentBackend {
    return {
      open: (name: string, bytes: Uint8Array): string => nativeBridge.openOriginalDocument(name, bytes),
      close: (): string => nativeBridge.closeOriginalDocument(),
      status: (): string => nativeBridge.getOriginalDocumentStatus(),
      snapshot: (): Uint8Array => nativeBridge.exportOriginalDocument()
    };
  }

  private readOriginalDocumentStatus(): void {
    try {
      const raw: string = nativeBridge.getOriginalDocumentStatus();
      if (raw !== this.originalDocumentRaw) {
        this.originalDocument = parseOriginalDocument(raw);
        this.originalDocumentRaw = raw;
      }
      this.originalDocumentStatusError = '';
      this.reconcileOriginalSessionNative();
    } catch (error) {
      this.originalDocumentStatusError = (error as Error).message || '原版文档缓存状态无法读取';
      this.reconcileOriginalSessionNative();
    }
  }

  private originalDocumentActive(): boolean {
    return this.originalDocument.sessionRetained === true || this.originalDocument.core.loaded || this.originalDocument.core.modelMayHaveChanged ||
      this.graphics.originalDocumentView === true;
  }

  private originalDocumentViewPresented(): boolean {
    if (this.originalDocumentStatusError.length > 0 || !this.graphics.surfaceReady || !this.graphics.initialized) return false;
    if (originalDocumentPresented(this.originalDocument)) {
      return this.originalDocumentSessions.matchesNative(this.originalDocument) &&
        this.graphics.originalDocumentView === true && this.graphics.error.length === 0;
    }
    // An ordinary main-frame action (for example the original grid handler
    // refusing a fixture without a map) can report a native action error while the
    // same source is still loaded. Keep the document toolbar usable and the error
    // visible instead of orphaning the document. Nothing is inferred when the
    // source is no longer loaded or has been replaced/modified.
    const core: OriginalDocumentCoreStatus = this.originalDocument.core;
    return core.loaded && core.error.length > 0 && core.name.length > 0 && core.byteCount > 0 &&
      core.frames > 0 && /^[0-9a-f]{64}$/i.test(core.sha256) &&
      this.originalDocumentSessions.matchesNativeSourceIdentity(this.originalDocument);
  }

  private acceptOriginalDocumentResult(result: OriginalDocumentTransferResult,
    saved?: OriginalDocumentSaveCompletion): void {
    this.originalDocumentTransferStatus = result.status; this.originalDocumentLastOperation = result.document;
    this.originalDocument = result.document;
    if (result.status.state === 'passed') {
      if (result.status.operation === 'import') {
        const bytes: Uint8Array = nativeBridge.readOriginalDocumentSource();
        const observed: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
        this.originalDocument = observed;
        if (!this.sameOriginalSource(result.document, observed)) throw new Error('新文档实际源读取后身份改变，旧标签和草稿保留');
        this.recordOriginalRecent(observed.core.name, bytes);
        const id: string = this.originalDocumentSessions.add(observed, bytes, result.status.sourceKind);
        const ui: OriginalDocumentSessionUi = { id: id, editor: new OriginalDocumentEditor(), history: new OriginalTextHistory(),
          editorVisible: false, findVisible: false, query: '', replacement: '', selectionStart: 0, selectionEnd: 0,
          message: '实际源文本；另存为经原版保存器规范化。', sourceKind: result.status.sourceKind,
          toolsAction: 'new-document', toolsSequence: 0, replaceCount: 0, viewportSequence: 0, viewportObservationKey: '' };
        this.originalDocumentSessionUi.set(id, ui); this.bindOriginalDocumentUi(id);
        this.initializeOriginalViewportForNewTab(id);
        this.originalDocumentSessionOperation.targetId = id;
      } else if (result.status.operation === 'export') {
        if (saved === undefined || saved.snapshot.byteCount !== result.status.byteCount) {
          throw new Error('文件写入已返回，但保存快照资格缺失；不能清除未保存状态');
        }
        const observed: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
        const id: string = this.requireOriginalSessionNative(observed);
        if (id !== saved.id || !this.sameOriginalSource(saved.nativeSource, observed) ||
          !this.sameOriginalSource(result.document, observed)) {
          throw new Error('保存后当前文档身份改变，不能把保存结果记给另一标签');
        }
        const target: OriginalDocumentSaveTarget = this.originalDocumentSaveTargets.recordSaved(id,
          result.status.selectedUri, observed.core.name, saved.snapshot.byteCount, saved.snapshot.sha256);
        this.originalDocument = observed;
        if (this.originalTextEditorState.initialized) this.originalTextEditor.markSaved(observed);
        this.originalDocumentSessions.markSaved(id); this.updateOriginalTextEditorState();
        this.originalDocumentSaveOperation.name = target.name;
        this.originalTextEditorMessage = `已保存到 ${target.name} 并逐字节回读核对；保存器可能规范化源文本，撤销历史保留。`;
      } else if (result.status.operation === 'close') {
        this.originalDocumentSessions.acceptClosed(result.document);
        // Actual Close restores the captured workbench camera before any neighbor reload.
        this.graphics = nativeBridge.getGraphicsStatus();
        this.yaw = this.graphics.yaw; this.pitch = this.graphics.pitch; this.distance = this.graphics.distance;
        this.captureDesktopStateReadback();
      }
      this.originalDocumentSessionOperation.state = 'passed';
      this.originalDocumentMessage = result.status.operation === 'export' ?
        `已由原版保存器导出，并逐字节回读核对 ${result.status.byteCount} 字节。` :
        result.status.operation === 'close' ? '当前原生文档已明确关闭；其他会话草稿仍保留。' :
        result.status.sourceKind === 'synthetic-platform-display' ?
          '已新增内置合成无机显示示例标签；仅平台显示，非科研数据。其他标签草稿保留。' :
          `已新增并呈现：${result.status.name}。其他标签草稿保留，未自动执行任何脚本。`;
    } else if (result.status.state === 'cancelled') {
      this.originalDocumentSessionOperation.state = 'cancelled';
      this.originalDocumentMessage = result.status.operation === 'export' ?
        '已取消另存为；现有保存目标、未保存状态、其他标签和撤销历史保留。' :
        '已取消系统文件选择，没有新增标签；现有文档、草稿和撤销历史保留。';
    } else {
      this.originalDocumentSessionOperation.state = 'failed';
      this.originalDocumentSessionOperation.error = result.status.error;
      this.originalDocumentMessage = result.status.error +
        (result.status.destinationMayBePartial ? ' 目标文件可能写入不完整，请检查。' : '') +
        (result.document.core.modelMayHaveChanged ? ' 原生核心报告模型可能改变，不能声称已回滚。' : '') +
        (result.status.nativeStatusError.length > 0 ? ' ' + result.status.nativeStatusError : '');
    }
    this.reconcileOriginalSessionNative();
  }

  private async transferOriginalDocument(operation: string): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (operation === 'close') {
      await this.closeOriginalDocumentSession(this.originalDocumentSessions.getStatus().activeId); return;
    }
    const saving: boolean = operation === 'save' || operation === 'export';
    const target: OriginalDocumentSaveTarget | undefined = this.originalDocumentSaveTargets.get(this.boundOriginalDocumentId);
    if (saving) {
      this.originalDocumentSaveOperation = { sequence: this.originalDocumentSaveOperation.sequence + 1,
        id: this.boundOriginalDocumentId, mode: operation === 'save' && target !== undefined ? 'saved-target' : 'save-as',
        state: 'preparing', selectedUri: '', name: '', byteCount: 0, sha256: '', writeStarted: false, error: '' };
    }
    if (saving && this.originalTextEditor.getStatus().draftDirty) {
      this.originalTextEditorVisible = true;
      this.originalTextEditorMessage = '请先应用草稿修改，再保存或另存为；没有写入任何文件。';
      this.originalDocumentSaveOperation.state = 'blocked-unapplied-draft';
      this.originalDocumentSaveOperation.error = this.originalTextEditorMessage;
      this.saveRecord(); return;
    }
    // Reserve the worst-case input before either the picker or a native load.
    if (operation === 'import' && !this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。没有打开文件选择器或替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation(operation, this.boundOriginalDocumentId);
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    this.desktopShortcutLatch.reset(); this.desktopMetaKeys.clear();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      if (saving) {
        const before: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
        const id: string = this.requireOriginalSessionNative(before);
        const transport: OriginalDocumentSaveTransport = new OriginalDocumentSaveTransport(
          this.originalDocumentBackend(), new HarmonyOriginalDocumentPort(context), originalDocumentSha256,
          (uri: string): void => {
            const current: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
            if (this.requireOriginalSessionNative(current) !== id || !this.sameOriginalSource(before, current)) {
              throw new Error('等待保存期间当前原生来源已改变，没有写入目标文件');
            }
            this.originalDocumentSaveTargets.checkDestination(id, uri, current.core.name);
          }, target, operation === 'save');
        const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.run('export', transport, transport);
        const snapshot: OriginalDocumentSerializedSnapshot | undefined = transport.getSnapshot();
        this.originalDocumentSaveOperation.state = result.status.state;
        this.originalDocumentSaveOperation.selectedUri = result.status.selectedUri;
        this.originalDocumentSaveOperation.writeStarted = transport.didStartWrite();
        this.originalDocumentSaveOperation.error = result.status.error;
        if (snapshot !== undefined) {
          this.originalDocumentSaveOperation.byteCount = snapshot.byteCount;
          this.originalDocumentSaveOperation.sha256 = snapshot.sha256;
        }
        if (result.status.state === 'failed') result.status.destinationMayBePartial = transport.didStartWrite();
        let completion: OriginalDocumentSaveCompletion | undefined = undefined;
        if (result.status.state === 'passed') {
          if (snapshot === undefined || snapshot.byteCount !== result.status.byteCount || !transport.didStartWrite()) {
            throw new Error('保存写入已返回，但快照或写入观测不一致；未清除文档脏状态');
          }
          completion = { id: id, mode: this.originalDocumentSaveOperation.mode, snapshot: snapshot, nativeSource: before };
        }
        this.acceptOriginalDocumentResult(result, completion);
      } else {
        if (operation === 'import') this.captureOriginalViewportBeforeReplacement('before-import');
        const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.run(operation,
          this.originalDocumentBackend(), new HarmonyOriginalDocumentPort(context));
        this.acceptOriginalDocumentResult(result);
      }
    } catch (error) {
      if (saving) {
        this.originalDocumentSaveOperation.state = 'failed';
        this.originalDocumentSaveOperation.error = (error as Error).message || '保存未确认完成';
      }
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openSyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-display.cif');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend());
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openShelxSyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-view.ins');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), 'synthetic-view.ins');
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openXyzSyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-view.xyz');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), 'synthetic-view.xyz');
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openPdbSyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-view.pdb');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), 'synthetic-view.pdb');
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openMolSyntheticOriginalDocument(): Promise<void> {
    await this.openRawSyntheticOriginalDocument('synthetic-view.mol');
  }

  private async openResSyntheticOriginalDocument(): Promise<void> {
    await this.openRawSyntheticOriginalDocument('synthetic-view.res');
  }

  private async openRawSyntheticOriginalDocument(fileName: string): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent(fileName);
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), fileName);
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openBondedSyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-bonded.cif');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), 'synthetic-bonded.cif');
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private async openSymmetrySyntheticOriginalDocument(): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未替换原生文档。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-symmetric', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      const bytes: Uint8Array = await context.resourceManager.getRawFileContent('synthetic-symmetry.cif');
      this.captureOriginalViewportBeforeReplacement('before-new-display-tab');
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.openSynthetic(bytes,
        this.originalDocumentBackend(), 'synthetic-symmetry.cif');
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private executeEditedPython(): void {
    if (this.pythonTransferBusy) return;
    try { encodePythonDocument(this.pythonEditorSource); }
    catch (error) { this.pythonEditorMessage = (error as Error).message; return; }
    this.executePythonSource(this.pythonEditorSource);
    this.pythonEditorMessage = this.pythonExecution.passed ? '脚本已执行；编辑内容未自动保存。' : '脚本执行未通过；详情见下方结果。';
  }

  private showPythonOutput(): void {
    const status: string = this.pythonRuntime.initialized ? `Python ${this.pythonRuntime.version} · 原应用桥接${this.pythonRuntime.originalBridgeImported ? '已导入' : '未导入'}` : `Python 未初始化：${this.pythonRuntime.error}`;
    AlertDialog.show({ title: 'Python 执行结果',
      message: status + '\n\n' + this.pythonExecution.output + (this.pythonExecution.error.length > 0 ? '\n' + this.pythonExecution.error : ''),
      confirm: { value: '关闭', action: () => {} } });
  }

  private executeApplicationFunction(): void {
    const sequence: number = this.applicationFunction.sequence + 1;
    try {
      const result: ApplicationFunctionReport = nativeBridge.evaluateApplicationFunction(this.commandText);
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

  private executeOriginalCommand(): void {
    const input: string = this.commandText;
    this.recordCommandHistory(input);
    try {
      const result: OriginalCommandReport = nativeBridge.executeOriginalCommand(input);
      this.applicationRuntime = result.runtime;
      this.commandPassed = result.passed;
      this.commandError = result.error;
      const output: string = result.output.trim();
      this.commandMessage = output.length > 0 ? output : (result.passed ? result.text : '原版命令未通过');
      this.originalLog = nativeBridge.getOriginalLogs();
      this.refreshGraphics();
      this.rebindPresentedSessionAfterModelChange();
      if (result.passed && this.originalDocumentActive()) {
        // Original commands can change renderer state without drawing. Redraw the
        // retained document so the effect is visible immediately; a redraw failure
        // is reported through the document/graphics status, not as a false failure.
        try {
          nativeBridge.redrawOriginalDocument();
          this.readOriginalDocumentStatus();
          this.refreshGraphics();
        } catch (redrawError) {
          this.originalLog = nativeBridge.getOriginalLogs();
        }
      }
    } catch (error) {
      this.commandPassed = false;
      this.commandMessage = '';
      this.commandError = '原版命令输入应为1～4096字节；请检查输入后重试';
    }
    this.saveRecord();
  }

  private showOriginalAtomInfo(): void {
    const count: number | undefined = this.originalDocument.core.selectedObjectCount;
    if (!this.originalSelectionEnabled() || count === undefined || count === 0) {
      this.originalSelectionMessage = '先选中至少一个原子，再查看原版原子信息';
      return;
    }
    try {
      const result: OriginalCommandReport = nativeBridge.executeOriginalCommand('Info');
      let text: string = result.output.trim();
      if (text.length === 0) text = result.text.length > 0 ? result.text : '';
      if (text.length === 0) text = result.passed ? '原版 Info 没有返回文本；选中对象可能不是原子。' : result.error;
      if (text.length > 4000) text = text.substring(0, 4000) + '\n…（已截断）';
      AlertDialog.show({ title: '原版原子信息', message: text,
        confirm: { value: '关闭', action: () => {} } });
    } catch (error) {
      AlertDialog.show({ title: '原版原子信息', message: (error as Error).message || '原版 Info 命令失败',
        confirm: { value: '关闭', action: () => {} } });
    }
  }

  private rebindPresentedSessionAfterModelChange(): void {
    // Model-changing original commands can alter atom/bond counts while the
    // source bytes and revision stay unchanged; refresh the presented identity
    // before the status read reconciles and would drop the native binding.
    try {
      const parsed: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
      this.originalDocumentSessions.refreshPresentedIdentity(parsed);
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版文档状态读回失败';
    }
  }

  private runOriginalSelectionCommand(command: string): void {
    if (!this.originalSelectionEnabled()) return;
    try {
      const result: OriginalCommandReport = nativeBridge.executeOriginalCommand(command);
      this.commandPassed = result.passed;
      this.commandError = result.error;
      const output: string = result.output.trim();
      this.commandMessage = output.length > 0 ? output : (result.passed ? result.text : '原版命令未通过');
      this.originalSelectionMessage = result.passed ?
        `已执行原版命令 ${command}` + (output.length > 0 ? '；输出见命令状态' : '') :
        (result.error || '原版命令未通过');
      if (result.passed) {
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版选择命令失败';
    }
  }

  private applyOriginalBondRadius(): void {
    if (!this.originalSelectionEnabled()) return;
    const value: number = Number(this.originalBondRadiusText.trim());
    if (!Number.isFinite(value) || value <= 0 || value > 5) {
      this.originalSelectionMessage = '键半径应为 0-5 之间的数字；未调用原版键半径设置';
      return;
    }
    try {
      const result: OriginalBondRadiusReport = nativeBridge.setOriginalBondRadius(value);
      if (!result.passed) {
        this.originalSelectionMessage = result.error || '原版键半径设置未通过';
        return;
      }
      this.originalSelectionMessage = `原版键半径已设置：${result.radius}`;
      try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版键半径设置调用失败';
    }
  }

  private recordOriginalRecent(name: string, bytes: Uint8Array): void {
    const key: string = name + ':' + originalDocumentSha256(bytes);
    if (this.originalRecentList.some((item) => item.key === key)) return;
    const entry: OriginalRecentDocument = { key: key, name: name, bytes: bytes };
    this.originalRecentList.push(entry);
    this.originalRecentLabels = this.originalRecentList.map((item) => item.name);
    if (this.originalRecentList.length > 8) {
      this.originalRecentList.shift();
      this.originalRecentLabels = this.originalRecentList.map((item) => item.name);
    }
  }

  private async openOriginalRecent(index: number): Promise<void> {
    if (this.originalDocumentOperationBusy() || !this.applicationRuntime.initialized) return;
    const item = this.originalRecentList[index];
    if (item === undefined) return;
    if (!this.originalDocumentSessions.canAdd()) {
      this.originalDocumentMessage = '会话最多 4 份文档、合计 32 MiB；请先明确关闭一份文档。未打开最近文件。';
      this.saveRecord(); return;
    }
    this.syncOriginalDocumentSessions(); this.beginOriginalSessionOperation('open-synthetic', '');
    this.originalDocumentBusy = true; this.syncOriginalViewportInput();
    try {
      const result: OriginalDocumentTransferResult = await this.originalDocumentTransfer.reopenRecorded(
        item.bytes, this.originalDocumentBackend(), item.name);
      this.acceptOriginalDocumentResult(result);
    } catch (error) {
      this.failOriginalSessionOperation(error as Error); this.readOriginalDocumentStatus(); this.reconcileOriginalSessionNative();
    } finally {
      this.originalDocumentBusy = false; this.syncOriginalDocumentSessions();
      this.applicationRuntime = nativeBridge.getApplicationRuntimeStatus();
      this.originalLog = nativeBridge.getOriginalLogs(); this.refreshGraphics(); this.syncOriginalViewportInput();
    }
  }

  private runOriginalDrawingStyle(macro: string, label: string): void {
    if (!this.originalSelectionEnabled()) return;
    this.runOriginalSelectionCommand(macro);
    this.originalSelectionMessage = `原版绘图样式：${label}（${macro}）` + (this.commandPassed ? '已执行' : '未通过');
    this.readOriginalDocumentStatus();
  }

  private openOriginalGenerateDialog(): void {
    if (this.generateDialogOpen || !this.originalDocumentActive()) return;
    try {
      const opened = nativeBridge.openOriginalGenerateDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版生成选项未打开';
        return;
      }
      this.generateDialogOpen = true;
      this.originalSelectionMessage = '已打开原版生成选项…';
      if (this.generateDialogTimer < 0) {
        this.generateDialogTimer = setInterval(() => { this.pollOriginalGenerateDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版生成选项调用失败';
    }
  }

  private pollOriginalGenerateDialog(): void {
    if (!this.generateDialogOpen) return;
    try {
      const report: OriginalGenerateDialogReport = nativeBridge.pollOriginalGenerateDialog();
      if (report.state === 'open') return;
      if (this.generateDialogTimer >= 0) clearInterval(this.generateDialogTimer);
      this.generateDialogTimer = -1;
      this.generateDialogOpen = false;
      if (report.state !== 'done') return;
      if (!report.accepted) {
        this.originalSelectionMessage = '已取消原版生成选项';
        return;
      }
      const values: number[] = report.values === undefined ? [] : report.values;
      if (values.length !== 6 || values.some((value: number) => !Number.isFinite(value))) {
        this.originalSelectionMessage = '原版生成选项未返回完整的 A/B/C 范围';
        return;
      }
      const command: string = `pack ${values[0]} ${values[1]} ${values[2]} ${values[3]} ${values[4]} ${values[5]}`;
      const result: OriginalCommandReport = nativeBridge.executeOriginalCommand(command);
      this.commandPassed = result.passed;
      this.commandError = result.error;
      const output: string = result.output.trim();
      this.commandMessage = output.length > 0 ? output : (result.passed ? result.text : '原版 pack 未通过');
      this.originalSelectionMessage = result.passed ? `已按原版生成选项执行 ${command}` : (result.error || '原版 pack 未通过');
      if (result.passed) {
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版生成选项轮询失败';
    }
  }

  private openOriginalPeriodicTable(): void {
    if (this.periodicTableOpen || !this.originalDocumentActive()) return;
    try {
      const opened = nativeBridge.openOriginalPeriodicTable();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版周期表未打开';
        return;
      }
      this.periodicTableOpen = true;
      this.originalSelectionMessage = '已打开原版周期表；点击元素完成选择';
      if (this.periodicTableTimer < 0) {
        this.periodicTableTimer = setInterval(() => { this.pollOriginalPeriodicTable(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版周期表调用失败';
    }
  }

  private pollOriginalPeriodicTable(): void {
    if (!this.periodicTableOpen) return;
    try {
      const report: OriginalPeriodicTableReport = nativeBridge.pollOriginalPeriodicTable();
      if (report.state === 'open') return;
      if (this.periodicTableTimer >= 0) clearInterval(this.periodicTableTimer);
      this.periodicTableTimer = -1;
      this.periodicTableOpen = false;
      if (report.state !== 'done') return;
      const symbol: string = report.symbol === undefined ? '' : report.symbol;
      if (!report.accepted || symbol.length === 0) {
        this.originalSelectionMessage = '已取消原版周期表选择';
        return;
      }
      // The original table ends on the element click; apply the same Name
      // command the quick element menu uses.
      this.runOriginalSelectionCommand(`name sel ${symbol}`);
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版周期表轮询失败';
    }
  }

  private openOriginalMaterialDialog(): void {
    if (this.materialDialogOpen || !this.originalSelectionEnabled()) return;
    try {
      const opened = nativeBridge.openOriginalMaterialDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版材质对话框未打开';
        return;
      }
      this.materialDialogOpen = true;
      this.originalSelectionMessage = '已打开原版材质对话框…';
      if (this.materialDialogTimer < 0) {
        this.materialDialogTimer = setInterval(() => { this.pollOriginalMaterialDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版材质对话框调用失败';
    }
  }

  private readOriginalBadReflections(): void {
    try {
      const report: OriginalBadReflectionsReport = nativeBridge.readOriginalBadReflections();
      if (!report.passed) {
        this.originalSelectionMessage = report.error || '原版坏反射读取失败';
        return;
      }
      if (report.count === 0) {
        this.originalSelectionMessage = `无坏反射（当前文档没有偏差反射列表；HKLF ${report.hklf}）`;
        return;
      }
      let preview: string = '';
      try {
        const parsed: Record<string, Object> = JSON.parse(report.json) as Record<string, Object>;
        const rows: Object[] = parsed['reflections'] as Object[];
        preview = rows.slice(0, 3).map((row: Object) => {
          const item: Record<string, number | boolean> = row as Record<string, number | boolean>;
          return `${item['h']} ${item['k']} ${item['l']} (${Number(item['factor']).toFixed(2)})`;
        }).join(', ');
      } catch (parseError) {
        preview = '';
      }
      this.originalSelectionMessage = `原版坏反射 ${report.count} 条（HKLF ${report.hklf}）` + (preview.length > 0 ? '：' + preview : '');
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版坏反射调用失败';
    }
  }

  private pollOriginalMaterialDialog(): void {
    if (!this.materialDialogOpen) return;
    try {
      const report: OriginalMaterialDialogReport = nativeBridge.pollOriginalMaterialDialog();
      if (report.state === 'open') return;
      if (this.materialDialogTimer >= 0) clearInterval(this.materialDialogTimer);
      this.materialDialogTimer = -1;
      this.materialDialogOpen = false;
      if (report.state !== 'done') return;
      if (!report.accepted) {
        this.originalSelectionMessage = '已取消原版材质对话框';
        return;
      }
      const applied: boolean = report.applied === true;
      const sceneChanged: boolean = report.sceneChanged === true;
      let message: string = `材质已应用：对话框已应用=${applied ? '是' : '否'}，场景已更新=${sceneChanged ? '是' : '否'}`;
      this.originalSelectionMessage = message;
      if (applied) {
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版材质对话框轮询失败';
    }
  }

  private runOriginalPartCommand(value: number): void {
    if (!this.originalSelectionEnabled()) return;
    try {
      const result: OriginalCommandReport = nativeBridge.executeOriginalCommand(`part ${value}`);
      this.commandPassed = result.passed;
      this.commandError = result.error;
      const output: string = result.output.trim();
      this.commandMessage = output.length > 0 ? output : (result.passed ? result.text : '原版 part 未通过');
      let parts: string = '';
      try {
        const parsed: OriginalDocumentEnvelope = parseOriginalDocument(nativeBridge.getOriginalDocumentStatus());
        parts = parsed.core.selectedParts === undefined ? '' : parsed.core.selectedParts;
      } catch (statusError) {
        parts = '';
      }
      this.originalSelectionMessage = result.passed ? `原版 Part=${value}；选中原子 Parts=${parts}` : (result.error || '原版 part 未通过');
      if (result.passed) {
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版 Part 命令失败';
    }
  }

  private openOriginalSceneDialog(): void {
    if (this.sceneDialogOpen || !this.originalDocumentActive()) return;
    try {
      const opened = nativeBridge.openOriginalSceneDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版场景设置未打开';
        return;
      }
      this.sceneDialogOpen = true;
      this.originalSelectionMessage = '已打开原版场景设置…';
      if (this.sceneDialogTimer < 0) {
        this.sceneDialogTimer = setInterval(() => { this.pollOriginalSceneDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版场景设置调用失败';
    }
  }

  private pollOriginalSceneDialog(): void {
    if (!this.sceneDialogOpen) return;
    try {
      const report: OriginalSceneDialogReport = nativeBridge.pollOriginalSceneDialog();
      if (report.state === 'open') return;
      if (this.sceneDialogTimer >= 0) clearInterval(this.sceneDialogTimer);
      this.sceneDialogTimer = -1;
      this.sceneDialogOpen = false;
      if (report.state !== 'done') return;
      if (!report.accepted) {
        this.originalSelectionMessage = '已取消原版场景设置';
        return;
      }
      const changed: boolean = report.sceneChanged === true;
      this.originalSelectionMessage = `场景已应用：场景已更新=${changed ? '是' : '否'}`;
      try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版场景设置轮询失败';
    }
  }

  private openOriginalGradientDialog(): void {
    if (this.gradientDialogOpen || !this.originalDocumentActive()) return;
    try {
      const opened = nativeBridge.openOriginalGradientDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版背景设置未打开';
        return;
      }
      this.gradientDialogOpen = true;
      this.originalSelectionMessage = '已打开原版背景设置…';
      if (this.gradientDialogTimer < 0) {
        this.gradientDialogTimer = setInterval(() => { this.pollOriginalGradientDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版背景设置调用失败';
    }
  }

  private pollOriginalGradientDialog(): void {
    if (!this.gradientDialogOpen) return;
    try {
      const report: OriginalGradientDialogReport = nativeBridge.pollOriginalGradientDialog();
      if (report.state === 'open') return;
      if (this.gradientDialogTimer >= 0) clearInterval(this.gradientDialogTimer);
      this.gradientDialogTimer = -1;
      this.gradientDialogOpen = false;
      if (report.state !== 'done') return;
      if (!report.accepted) {
        this.originalSelectionMessage = '已取消原版背景设置';
        return;
      }
      const applied: boolean = report.applied === true;
      const uncertain: boolean = report.uncertain === true;
      this.originalSelectionMessage = `背景已应用：已应用=${applied ? '是' : '否'}，不确定=${uncertain ? '是' : '否'}`;
      if (applied) {
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版背景设置轮询失败';
    }
  }

  private openOriginalBondLengthDialog(): void {
    if (this.bondLengthDialogOpen) return;
    try {
      const opened = nativeBridge.openOriginalBondLengthDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版键长未打开';
        return;
      }
      this.bondLengthDialogOpen = true;
      this.originalSelectionMessage = '已打开原版键长…';
      if (this.bondLengthDialogTimer < 0) {
        this.bondLengthDialogTimer = setInterval(() => { this.pollOriginalBondLengthDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版键长调用失败';
    }
  }

  private pollOriginalBondLengthDialog(): void {
    if (!this.bondLengthDialogOpen) return;
    try {
      const report: OriginalBondLengthDialogReport = nativeBridge.pollOriginalBondLengthDialog();
      if (report.state === 'open') return;
      if (this.bondLengthDialogTimer >= 0) clearInterval(this.bondLengthDialogTimer);
      this.bondLengthDialogTimer = -1;
      this.bondLengthDialogOpen = false;
      if (report.state !== 'done') return;
      if (report.applied) {
        this.originalSelectionMessage = `原版键长已修改：${report.length}`;
        try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
        this.readOriginalDocumentStatus();
        this.refreshGraphics();
      } else {
        this.originalSelectionMessage = report.error ? (report.error || '原版键长未应用') : '已取消原版键长';
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版键长轮询失败';
    }
  }

  private openOriginalUpdateOptionsDialog(): void {
    if (this.updateOptionsDialogOpen) return;
    try {
      const opened = nativeBridge.openOriginalUpdateOptionsDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版更新选项未打开';
        return;
      }
      this.updateOptionsDialogOpen = true;
      this.originalSelectionMessage = '已打开原版更新选项…';
      if (this.updateOptionsDialogTimer < 0) {
        this.updateOptionsDialogTimer = setInterval(() => { this.pollOriginalUpdateOptionsDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版更新选项调用失败';
    }
  }

  private pollOriginalUpdateOptionsDialog(): void {
    if (!this.updateOptionsDialogOpen) return;
    try {
      const report: OriginalUpdateOptionsDialogReport = nativeBridge.pollOriginalUpdateOptionsDialog();
      if (report.state === 'open') return;
      if (this.updateOptionsDialogTimer >= 0) clearInterval(this.updateOptionsDialogTimer);
      this.updateOptionsDialogTimer = -1;
      this.updateOptionsDialogOpen = false;
      if (report.state !== 'done') return;
      this.originalSelectionMessage = report.accepted ? '原版更新选项已应用' : '已取消原版更新选项';
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版更新选项轮询失败';
    }
  }

  private openOriginalPrimitiveDialog(): void {
    if (this.primitiveDialogOpen || !this.originalSelectionEnabled()) return;
    try {
      const opened = nativeBridge.openOriginalPrimitiveDialog();
      if (!opened.opened) {
        this.originalSelectionMessage = opened.error || '原版图元对话框未打开';
        return;
      }
      this.primitiveDialogOpen = true;
      this.originalSelectionMessage = '已打开原版图元对话框…';
      if (this.primitiveDialogTimer < 0) {
        this.primitiveDialogTimer = setInterval(() => { this.pollOriginalPrimitiveDialog(); }, 400);
      }
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版图元对话框调用失败';
    }
  }

  private pollOriginalPrimitiveDialog(): void {
    if (!this.primitiveDialogOpen) return;
    try {
      const report: OriginalPrimitiveDialogReport = nativeBridge.pollOriginalPrimitiveDialog();
      if (report.state === 'open') return;
      if (this.primitiveDialogTimer >= 0) clearInterval(this.primitiveDialogTimer);
      this.primitiveDialogTimer = -1;
      this.primitiveDialogOpen = false;
      if (report.state !== 'done') return;
      if (!report.accepted) {
        this.originalSelectionMessage = '已取消原版图元对话框';
        return;
      }
      const applied: boolean = report.applied === true;
      const mask: number = report.mask === undefined ? -1 : report.mask;
      this.originalSelectionMessage = `图元已应用：mask=${mask}，已应用=${applied ? '是' : '否'}`;
      try { nativeBridge.redrawOriginalDocument(); } catch (redrawError) { this.originalLog = nativeBridge.getOriginalLogs(); }
      this.rebindPresentedSessionAfterModelChange();
      this.readOriginalDocumentStatus();
      this.refreshGraphics();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '原版图元对话框轮询失败';
    }
  }

  private recordCommandHistory(input: string): void {
    const value: string = input.trim();
    if (value.length === 0) return;
    if (this.commandHistory.length === 0 || this.commandHistory[this.commandHistory.length - 1] !== value) {
      this.commandHistory.push(value);
      if (this.commandHistory.length > 20) this.commandHistory.shift();
    }
    this.commandHistoryIndex = -1;
  }

  private commandHistoryStep(previous: boolean): void {
    if (this.commandHistory.length === 0) return;
    if (this.commandHistoryIndex < 0) this.commandHistoryIndex = this.commandHistory.length;
    if (previous) this.commandHistoryIndex = Math.max(0, this.commandHistoryIndex - 1);
    else this.commandHistoryIndex = Math.min(this.commandHistory.length, this.commandHistoryIndex + 1);
    this.commandText = this.commandHistoryIndex >= this.commandHistory.length ? '' : this.commandHistory[this.commandHistoryIndex];
  }

  private executeDesktopCommand(): void {
    const input: string = this.commandText;
    this.recordCommandHistory(input);
    const sequence: number = this.commandStatus.sequence + 1;
    try {
      const result: DesktopCommandReport = nativeBridge.executeDesktopCommand(input);
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
    this.readOriginalDocumentStatus();
    this.observeOriginalDocumentViewport();
    this.scheduleOriginalDocumentRecovery();
    this.scheduleDesktopState();
    const record: RuntimeRecord = {
      installedPackage: { bundleName: this.bundleName, versionName: this.versionName, versionCode: this.versionCode,
        error: this.packageIdentityError, source: 'bundleManager.getBundleInfoForSelfSync' },
      scope: this.originalDocumentActive() ? '原 TGXApp/TGXFile 文档 + 原 TGlRenderer 有限显示 / 平台文件流程；非完整原界面或 Olex2' : '真实基础层 + 上游图元 / 新 GLES3 适配；非完整原版渲染器或 Olex2',
      utilityPassed: this.checks.passed, checkCount: this.checks.checks.length, checks: this.checks.checks,
      graphics: this.graphics, upstreamGeometryTested: this.graphics.meshValidated && this.graphics.meshSource.length > 0, upstreamSubmissionTested: this.graphics.submissionValidated && this.graphics.submissionSource.length > 0, originalOlex2RendererTested: false, fullPortComplete: false, keyEvents: this.keyEvents, axisEvents: this.axisEvents, viewportHovered: this.viewportHovered,
      workspaceState: { restored: this.stateLoadStatus === 'restored', loadStatus: this.stateLoadStatus,
        saveStatus: this.stateSaveStatus, schemaVersion: 4,
        readbackStatus: this.desktopReadbackStatus, readbackState: this.desktopReadbackState,
        uiYaw: this.yaw, uiPitch: this.pitch, uiDistance: this.distance },
      desktopCommand: this.commandStatus,
      applicationRuntime: this.applicationRuntime,
      applicationFunction: this.applicationFunction,
      pythonRuntime: this.pythonRuntime, pythonExecution: this.pythonExecution,
      pythonEditor: { visible: this.pythonEditorVisible, dirty: this.pythonEditorDirty, busy: this.pythonTransferBusy,
        characters: this.pythonEditorSource.length, document: this.pythonDocument },
      originalLog: this.originalLogAudit(), coreCatalog: this.coreCatalog, originalLogView: this.originalLogView,
      optionsDocument: this.optionsDocument,
      originalDocument: this.originalDocument,
      originalDocumentSessions: this.originalDocumentSessionsStatus,
      originalDocumentSaveTargets: this.originalDocumentSaveTargetSummaries,
      originalDocumentSaveOperation: this.originalDocumentSaveOperation,
      originalDocumentRecovery: { state: this.originalRecoveryState, pendingDecision: this.originalRecoveryPending,
        busy: this.originalRecoveryBusy, restoredDocumentCount: this.originalRecoveryRestoredCount,
        temporaryNativeRetained: this.originalRecoveryTemporaryNative,
        action: this.originalRecoveryAction, actionSequence: this.originalRecoveryActionSequence,
        detail: this.originalRecoveryMessage, error: this.originalRecoveryError, journal: this.originalRecoveryStatus },
      originalDocumentSessionTabs: this.originalDocumentTabs,
      originalDocumentSessionEditors: this.originalDocumentSessionEditorAudit(),
      originalDocumentViewport: this.originalDocumentViewportAudit,
      originalDocumentSessionOperation: this.originalDocumentSessionOperation,
      originalDocumentEditorGuardTargetId: this.originalTextGuardTargetId,
      originalDocumentTransfer: this.originalDocumentTransferStatus,
      originalDocumentLastOperation: this.originalDocumentLastOperation,
      originalDocumentEditor: this.originalTextEditorState, originalDocumentEditorVisible: this.originalTextEditorVisible,
      originalDocumentTextTools: { history: this.originalTextHistory.getStatus(), findVisible: this.originalTextFindVisible,
        goToLineVisible: this.originalTextGoToLineVisible, goToLineCharacters: this.originalTextGoToLineInput.length,
        sourceFocused: this.originalTextFocusedControl === 'source', queryCharacters: this.originalTextQuery.length,
        replacementCharacters: this.originalTextReplacement.length, selectionStart: this.originalTextSelectionStart,
        selectionEnd: this.originalTextSelectionEnd, action: this.originalTextToolsAction, sequence: this.originalTextToolsSequence,
        replaceCount: this.originalTextReplaceCount, shortcutAction: this.originalDocumentShortcutAction,
        shortcutSequence: this.originalDocumentShortcutSequence },
      originalDocumentEditorMessage: this.originalTextEditorMessage,
      originalDocumentEditorGuardBusy: this.originalTextGuardBusy,
      originalDocumentEditorGuardDecision: this.originalTextGuardDecision,
      originalDocumentEditorGuardSequence: this.originalTextGuardSequence,
      originalDocumentSourceKind: this.originalDocumentSourceKind,
      originalDocumentStatusError: this.originalDocumentStatusError,
      originalSelectionMessage: this.originalSelectionMessage, originalSelectionBusy: this.originalSelectionBusy,
      originalViewportFocused: this.originalViewportFocused, originalSelectionMode: this.originalSelectionMode
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

  private captureDesktopStateReadback(): void {
    // Normal in-application file API, not a shell pathname/permission workaround.
    // Called only at startup, actual writes and explicit document Close.
    const loaded = loadDesktopState(this.filesDir);
    this.desktopReadbackStatus = loaded.status;
    this.desktopReadbackState = loaded.status === 'restored' ? loaded.state : undefined;
  }

  private restoreDesktopState(): void {
    const loaded = loadDesktopState(this.filesDir);
    this.desktopReadbackStatus = loaded.status;
    this.desktopReadbackState = loaded.status === 'restored' ? loaded.state : undefined;
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
    // Do not save original-document camera/invalidated mesh diagnostics over the
    // separately persisted demo workspace. Existing settings remain untouched.
    if (this.originalDocumentActive() || !this.graphics.initialized || this.graphics.error.length > 0) return;
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
    this.captureDesktopStateReadback();
  }

  private setView(): void {
    if (!this.viewportInputAllowed()) return;
    this.graphics = nativeBridge.setView(this.yaw, this.pitch, this.distance);
    this.yaw = this.graphics.yaw; this.pitch = this.graphics.pitch; this.distance = this.graphics.distance;
    this.saveRecord();
  }

  private updateMesh(generation: number, wireframe: boolean): void {
    if (this.originalDocumentActive() || this.originalDocumentBusy) return;
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
    if (this.originalDocumentActive() || this.originalDocumentBusy) return;
    this.graphics = nativeBridge.setMaterialPreset(preset);
    this.saveRecord();
  }
  private selectColor(preset: number): void {
    if (this.originalDocumentActive() || this.originalDocumentBusy) return;
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
    if (this.originalDocumentActive() || this.originalDocumentBusy) return;
    this.graphics = nativeBridge.setPrimitive(kind);
    this.saveRecord();
  }
  private desktopContentWidth(): number {
    return desktopContentWidth(this.desktopWidth, this.sidebarVisible, this.desktopHeight);
  }

  private revealDesktopCommandInput(): void {
    if (!this.sidebarCommandFocusPending || !this.sidebarVisible ||
      this.sidebarContentHeight <= 0 || this.sidebarViewportHeight <= 0) return;
    this.sidebarCommandFocusPending = false;
    try {
      // Reveal measured command controls before focus; never submit an interpreter command.
      this.sidebarScroller.scrollTo({ xOffset: 0, yOffset: this.sidebarContentHeight, animation: false });
      this.getUIContext().getFocusController().requestFocus('olex2-command-input');
    } catch (error) {
      this.commandError = '无法聚焦命令输入：' + ((error as Error).message || '请使用侧栏命令输入框。');
      this.saveRecord();
    }
  }

  private showDesktopScope(): void {
    this.getUIContext().showAlertDialog({ title: '平台适配范围',
      message: '当前是鸿蒙 PC 模拟器开发验证版，不是完整原版功能或正式发行版。\n\n完整原版界面、菜单、命令与插件、多窗口、广泛格式、更多 DPI 和物理键鼠/输入法、长期稳定性及正式签名发行仍需完成。\n\n源码纳入编译的比例不是整体功能完成率。',
      confirm: { value: '知道了', action: () => {} } });
  }

  private currentDesktopMenuIds(): string[] {
    return enabledDesktopMenuIds(this.desktopDocumentActionsBusy(), this.originalTextEditorVisible,
      this.originalDocumentViewPresented());
  }

  private reportDesktopMenuFocusError(error: Error): void {
    const message: string = error.message || '菜单焦点未完成；请点击需要的菜单或工作区。';
    if (this.originalTextEditorVisible) this.originalTextEditorMessage = message;
    else this.commandMessage = message;
  }

  private revealDesktopMenuItem(id: string): void {
    if (this.desktopMenuViewportWidth <= 0) return;
    try {
      const current: number = this.desktopMenuScroller.currentOffset().xOffset;
      const target: number | undefined = desktopMenuScrollOffset(id, this.desktopMenuViewportWidth, current);
      if (target !== undefined && (!Number.isFinite(current) || Math.abs(target - current) > 0.5)) {
        this.desktopMenuScroller.scrollTo({ xOffset: target, yOffset: 0, animation: false });
      }
    } catch (error) { this.reportDesktopMenuFocusError(error as Error); }
  }

  private measureDesktopMenu(area: Area): void {
    const width: number = Number(area.width);
    this.desktopMenuViewportWidth = Number.isFinite(width) ? Math.max(0, width - 16) : 0;
    // Resizing may reveal the focused button; it must never request new focus.
    if (this.desktopMenuFocused) this.revealDesktopMenuItem(this.focusedDesktopMenuId);
  }

  private desktopMenuItemFocused(id: string): void {
    this.desktopMenuFocused = true; this.focusedDesktopMenuId = id;
    this.revealDesktopMenuItem(id);
  }

  private focusDesktopMenuItem(id: string): void {
    if (this.currentDesktopMenuIds().indexOf(id) < 0) return;
    this.revealDesktopMenuItem(id);
    try { this.getUIContext().getFocusController().requestFocus(id); }
    catch (error) { this.reportDesktopMenuFocusError(error as Error); }
  }

  private focusDesktopMenu(): void {
    if (this.desktopDocumentActionsBusy()) return;
    // Record the actual editable receiver before the menu's focus blurs it.
    if (!this.desktopMenuFocused) this.desktopMenuReturnControl = this.originalTextFocusedControl;
    this.focusDesktopMenuItem('olex2-menu-file');
  }

  private returnFromDesktopMenu(): void {
    if (this.desktopDocumentActionsBusy()) return;
    this.desktopMenuFocused = false;
    if (this.originalTextEditorVisible && !this.originalTextEditorState.initialized) {
      try { this.getUIContext().getFocusController().requestFocus('olex2-document-editor-back'); }
      catch (error) { this.reportDesktopMenuFocusError(error as Error); }
      return;
    }
    if (this.originalTextEditorVisible) {
      const control: string = this.originalTextFindVisible &&
        (this.desktopMenuReturnControl === 'query' || this.desktopMenuReturnControl === 'replacement') ?
        this.desktopMenuReturnControl : 'source';
      this.focusOriginalTextControl(this.originalTextGoToLineVisible && this.desktopMenuReturnControl === 'line' ?
        'olex2-document-editor-go-to-line-input' : control === 'source' ? 'olex2-document-editor-source' :
        control === 'query' ? 'olex2-document-editor-find-query' : 'olex2-document-editor-find-replacement');
      return;
    }
    const target: string = this.activePanel === 0 ? 'olex2-viewport-input' :
      this.activePanel === 1 ? 'olex2-checks-scroll' : 'olex2-records-scroll';
    try { this.getUIContext().getFocusController().requestFocus(target); }
    catch (error) { this.reportDesktopMenuFocusError(error as Error); }
  }

  private desktopMenuKey(event: KeyEvent): boolean {
    if (!this.desktopMenuFocused || this.desktopDocumentActionsBusy() ||
      event.type !== KeyType.Down || !event.getModifierKeyState) return false;
    const action: string = resolveDesktopMenuNavigation(event.keyCode,
      event.getModifierKeyState(['ctrl']), event.getModifierKeyState(['alt']),
      event.getModifierKeyState(['shift']), this.desktopMetaKeys.size > 0);
    if (action.length === 0) return false;
    if (action === 'return') {
      // Use the same held-Escape latch as Find so a repeated key cannot dismiss
      // another layer immediately after returning to the editor.
      if (this.originalDocumentShortcutLatch.press(event.keyCode)) this.returnFromDesktopMenu();
    } else {
      const id: string = desktopMenuFocusTarget(this.currentDesktopMenuIds(), this.focusedDesktopMenuId, action);
      if (id.length === 0) return false;
      this.focusDesktopMenuItem(id);
    }
    event.stopPropagation(); return true;
  }

  private desktopDocumentActionsBusy(): boolean {
    return this.originalDocumentOperationBusy() || this.pythonEditorVisible ||
      this.optionsTransferBusy || this.pythonTransferBusy;
  }

  private desktopDocumentLabel(id: string): string {
    const session: OriginalDocumentSessionSummary | undefined = this.originalDocumentTabs.find(
      (candidate: OriginalDocumentSessionSummary): boolean => candidate.id === id);
    if (session === undefined) return '文档已关闭';
    const target: OriginalDocumentSaveTarget | undefined = this.originalDocumentSaveTargetSummaries.find(
      (candidate: OriginalDocumentSaveTarget): boolean => candidate.id === id);
    return desktopDocumentMenuLabel(target !== undefined && target.name.length > 0 ? target.name : session.name,
      session.active, session.draftDirty, session.documentDirty);
  }

  private focusDesktopDocument(id: string): void {
    // A delayed focus request must not focus a tab closed or replaced in the meantime.
    if (this.desktopDocumentActionsBusy() || this.originalDocumentSessions.getStatus().activeId !== id ||
      !this.originalDocumentViewPresented()) return;
    if (this.originalTextEditorVisible) {
      this.focusOriginalTextControl('olex2-document-editor-source');
    } else if (this.activePanel === 0) {
      try { this.getUIContext().getFocusController().requestFocus('olex2-viewport-input'); }
      catch (error) {
        this.originalDocumentMessage = '标签已切换；显示区焦点未恢复：' + ((error as Error).message || '请点击显示区。');
        this.saveRecord();
      }
    }
  }

  private activateDesktopDocument(id: string): void {
    if (this.desktopDocumentActionsBusy() || !this.originalDocumentTabs.some(
      (candidate: OriginalDocumentSessionSummary): boolean => candidate.id === id)) return;
    // Keep the established per-document draft/history/viewport ownership transaction.
    this.activateOriginalDocumentSession(id);
    setTimeout(() => this.focusDesktopDocument(id), 0);
  }

  private navigateDesktopDocument(direction: number): void {
    if (this.desktopDocumentActionsBusy()) return;
    const id: string = adjacentDesktopDocumentId(this.originalDocumentTabs.map(
      (session: OriginalDocumentSessionSummary): string => session.id),
      this.originalDocumentSessions.getStatus().activeId, direction);
    if (id.length > 0) this.activateDesktopDocument(id);
  }

  private desktopEditorAction(action: string): void {
    if (this.desktopDocumentActionsBusy()) return;
    if (action === 'document-view') {
      this.originalTextEditorVisible = false; this.originalTextFocusedControl = '';
      this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = '';
      this.syncOriginalDocumentSessions(); this.syncOriginalViewportInput(); this.saveRecord();
      return;
    }
    if (!this.originalDocumentViewPresented()) return;
    if (!this.originalTextEditorVisible) this.openOriginalTextEditor();
    if (!this.originalTextEditorVisible || !this.originalTextEditorState.initialized) return;
    if (action === 'text-undo' || action === 'text-redo') {
      this.originalTextHistoryAction(action === 'text-undo' ? 'undo' : 'redo');
    } else if (action === 'text-find' || action === 'text-replace') {
      this.showOriginalTextFind(true);
      if (action === 'text-replace') this.focusOriginalTextControl('olex2-document-editor-find-replacement');
    } else if (action === 'text-find-next') this.findNextOriginalText();
    else if (action === 'text-find-previous') this.findPreviousOriginalText();
    else if (action === 'text-go-to-line') this.showOriginalTextGoToLine(true);
    else if (action === 'text-apply') this.applyOriginalText();
  }

  private desktopAction(action: string): void {
    if (action === 'options-import' || action === 'options-export') {
      if (!this.applicationRuntime.initialized || this.optionsTransferBusy) return;
      this.desktopShortcutLatch.reset();
      this.transferOptionsDocument(action === 'options-import' ? 'import' : 'export');
    } else if (action === 'focus-command') {
      this.sidebarVisible = true;
      this.sidebarCommandFocusPending = true;
      setTimeout(() => this.revealDesktopCommandInput(), 0);
    } else if (action === 'toggle-sidebar') {
      this.sidebarVisible = !this.sidebarVisible;
      if (!this.sidebarVisible) {
        this.sidebarCommandFocusPending = false;
        this.sidebarContentHeight = 0; this.sidebarViewportHeight = 0;
      }
    } else if (action === 'reset-view') {
      this.resetView();
    } else if (action === 'panel-0' || action === 'panel-1' || action === 'panel-2') {
      this.originalLogView = false;
      this.activePanel = action === 'panel-0' ? 0 : action === 'panel-1' ? 1 : 2;
    } else if (action === 'original-log') {
      this.activePanel = 2;
      this.originalLogView = true;
      this.originalLog = nativeBridge.getOriginalLogs();
    } else if (action === 'shortcut-help') {
      this.shortcutHelpVisible = !this.shortcutHelpVisible;
    }
  }

  private originalDocumentAction(action: string): void {
    if (this.desktopDocumentActionsBusy()) return;
    if (action === 'desktop-menu-focus') this.focusDesktopMenu();
    else if (action === 'document-next-tab' || action === 'document-previous-tab') {
      this.navigateDesktopDocument(action === 'document-next-tab' ? 1 : -1);
    } else if (action === 'document-open') this.transferOriginalDocument('import');
    else if (action === 'document-save') {
      if (this.originalDocument.core.loaded) this.transferOriginalDocument('save');
      else { this.originalDocumentMessage = '请先打开文档；没有写入任何文件。'; this.saveRecord(); }
    } else if (action === 'document-save-as') this.transferOriginalDocument('export');
    else if (action === 'document-close') {
      if (this.originalDocumentActive()) this.transferOriginalDocument('close');
    } else if (action === 'document-edit') this.openOriginalTextEditor();
    else if (action === 'text-undo' || action === 'text-redo') this.originalTextHistoryAction(action === 'text-undo' ? 'undo' : 'redo');
    else if (action === 'text-find' || action === 'text-replace' || action === 'text-go-to-line') this.desktopEditorAction(action);
    else if (action === 'text-find-next') this.findNextOriginalText();
    else if (action === 'text-find-previous') this.findPreviousOriginalText();
    else if (action === 'text-find-close') this.showOriginalTextFind(false);
    else if (action === 'text-go-to-line-close') this.showOriginalTextGoToLine(false);
    else if (action === 'options-import' || action === 'options-export') this.desktopAction(action);
  }

  private originalDocumentKey(event: KeyEvent): boolean {
    if (event.keyCode === KeyCode.KEYCODE_META_LEFT || event.keyCode === KeyCode.KEYCODE_META_RIGHT) {
      if (event.type === KeyType.Down) this.desktopMetaKeys.add(event.keyCode);
      else if (event.type === KeyType.Up) this.desktopMetaKeys.delete(event.keyCode);
      return false;
    }
    if (event.type === KeyType.Up) {
      this.originalDocumentShortcutLatch.release(event.keyCode);
      this.desktopDocumentNavigationLatch.release(event.keyCode);
      return false;
    }
    if (event.type !== KeyType.Down || !event.getModifierKeyState) return false;
    const action: string = resolveOriginalDocumentShortcut(event.keyCode,
      event.getModifierKeyState(['ctrl']), event.getModifierKeyState(['alt']),
      event.getModifierKeyState(['shift']), this.desktopMetaKeys.size > 0,
      { enabled: !this.desktopDocumentActionsBusy(),
        editorVisible: this.originalTextEditorVisible, sourceFocused: this.originalTextFocusedControl === 'source',
        findVisible: this.originalTextFindVisible, goToLineVisible: this.originalTextGoToLineVisible,
        tabCount: this.originalDocumentTabs.length });
    if (action.length === 0) return false;
    // Tab activation resets the editor shortcut latch. Keep navigation's held-key
    // state separate so a held Ctrl+Tab cannot traverse repeatedly after a rebind.
    const latch: DesktopShortcutLatch = action === 'document-next-tab' || action === 'document-previous-tab' ?
      this.desktopDocumentNavigationLatch : this.originalDocumentShortcutLatch;
    if (latch.press(event.keyCode)) {
      this.originalDocumentShortcutAction = action; this.originalDocumentShortcutSequence++;
      this.originalDocumentAction(action); this.saveRecord();
    }
    event.stopPropagation(); return true;
  }

  private desktopKey(event: KeyEvent): boolean {
    if (this.pythonEditorVisible || this.originalTextEditorVisible || this.originalTextGuardBusy) return false;
    // Ctrl+O/S are document commands handled at the outer dispatch boundary;
    // do not fall back to the historic settings shortcuts during a busy state.
    if (event.keyCode === KeyCode.KEYCODE_O || event.keyCode === KeyCode.KEYCODE_S) return false;
    if (event.keyCode === KeyCode.KEYCODE_META_LEFT || event.keyCode === KeyCode.KEYCODE_META_RIGHT) {
      if (event.type === KeyType.Down) this.desktopMetaKeys.add(event.keyCode);
      else if (event.type === KeyType.Up) this.desktopMetaKeys.delete(event.keyCode);
      return false;
    }
    if (event.type === KeyType.Up) {
      this.desktopShortcutLatch.release(event.keyCode);
      return false;
    }
    if (event.type !== KeyType.Down || !event.getModifierKeyState) return false;
    const action: string = resolveDesktopShortcut(event.keyCode,
      event.getModifierKeyState(['ctrl']), event.getModifierKeyState(['alt']),
      event.getModifierKeyState(['shift']), this.desktopMetaKeys.size > 0);
    if (action.length === 0) return false;
    event.stopPropagation();
    if (this.desktopShortcutLatch.press(event.keyCode)) this.desktopAction(action);
    return true;
  }

  @Builder
  private DesktopEditMenu() {
    Menu() {
      MenuItem({ content: this.originalTextEditorVisible ? '返回文档视图' : '编辑原文档文本',
        labelInfo: this.originalTextEditorVisible ? '' : 'Ctrl+E' })
        .id('olex2-edit-menu-toggle').enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentViewPresented())
        .onClick(() => {
          if (this.originalTextEditorVisible) this.desktopEditorAction('document-view');
          else this.originalDocumentAction('document-edit');
        })
      MenuItem({ content: '撤销', labelInfo: 'Ctrl+Z' }).id('olex2-edit-menu-undo')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalTextEditorState.initialized && this.originalTextCanUndo)
        .onClick(() => this.desktopEditorAction('text-undo'))
      MenuItem({ content: '重做', labelInfo: 'Ctrl+Y' }).id('olex2-edit-menu-redo')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalTextEditorState.initialized && this.originalTextCanRedo)
        .onClick(() => this.desktopEditorAction('text-redo'))
      MenuItem({ content: '查找', labelInfo: 'Ctrl+F' }).id('olex2-edit-menu-find')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentViewPresented())
        .onClick(() => this.desktopEditorAction('text-find'))
      MenuItem({ content: '替换', labelInfo: 'Ctrl+H' }).id('olex2-edit-menu-replace')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentViewPresented())
        .onClick(() => this.desktopEditorAction('text-replace'))
      MenuItem({ content: '查找下一个', labelInfo: 'F3' }).id('olex2-edit-menu-find-next')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalTextEditorState.initialized && this.originalTextQuery.length > 0)
        .onClick(() => this.desktopEditorAction('text-find-next'))
      MenuItem({ content: '查找上一个', labelInfo: 'Shift+F3' }).id('olex2-edit-menu-find-previous')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalTextEditorState.initialized && this.originalTextQuery.length > 0)
        .onClick(() => this.desktopEditorAction('text-find-previous'))
      MenuItem({ content: '跳转到行…', labelInfo: 'Ctrl+G' }).id('olex2-edit-menu-go-to-line')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentViewPresented())
        .onClick(() => this.desktopEditorAction('text-go-to-line'))
      MenuItem({ content: '应用修改（不自动保存）' }).id('olex2-edit-menu-apply')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalTextEditorState.initialized && this.originalTextEditorState.draftDirty)
        .onClick(() => this.desktopEditorAction('text-apply'))
    }
  }

  @Builder
  private DesktopDocumentsMenu() {
    Menu() {
      MenuItem({ content: '下一个文档', labelInfo: 'Ctrl+Tab' }).id('olex2-documents-menu-next')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentTabs.length > 1)
        .onClick(() => this.navigateDesktopDocument(1))
      MenuItem({ content: '上一个文档', labelInfo: 'Ctrl+Shift+Tab' }).id('olex2-documents-menu-previous')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentTabs.length > 1)
        .onClick(() => this.navigateDesktopDocument(-1))
      MenuItem({ content: '关闭当前文档…', labelInfo: 'Ctrl+W' }).id('olex2-documents-menu-close')
        .enabled(!this.desktopDocumentActionsBusy() && this.originalDocumentActive())
        .onClick(() => this.originalDocumentAction('document-close'))
      if (this.originalDocumentTabs.length === 0) {
        MenuItem({ content: '当前没有打开的文档' }).enabled(false)
      } else {
        ForEach(this.originalDocumentTabs, (session: OriginalDocumentSessionSummary) => {
          MenuItem({ content: this.desktopDocumentLabel(session.id) }).id(`olex2-documents-menu-${session.id}`)
            .enabled(!this.desktopDocumentActionsBusy())
            .onClick(() => this.activateDesktopDocument(session.id))
        }, (session: OriginalDocumentSessionSummary): string => session.id)
      }
    }
  }

  @Builder
  private DesktopMenuBar() {
    Column({ space: 0 }) {
      Scroll(this.desktopMenuScroller) {
      Row({ space: 4 }) {
      Button('文件').id('olex2-menu-file').width(desktopMenuButtonWidth('olex2-menu-file')).height(30)
        .onFocus(() => this.desktopMenuItemFocused('olex2-menu-file')).onBlur(() => { this.desktopMenuFocused = false; }).fontSize(12).enabled(!this.desktopDocumentActionsBusy())
        .fontColor('#000000').backgroundColor('#F0F0F0')
        .bindMenu([
          { value: '打开原版文档 .cif / .ins / .res…  Ctrl+O', enabled: this.applicationRuntime.initialized && !this.originalDocumentOperationBusy(),
            action: () => this.originalDocumentAction('document-open') },
          { value: '编辑原文档文本…  Ctrl+E', enabled: this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy(),
            action: () => this.originalDocumentAction('document-edit') },
          { value: '保存原版文档  Ctrl+S', enabled: this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy(),
            action: () => this.originalDocumentAction('document-save') },
          { value: '原版文档另存为…', enabled: this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy(),
            action: () => this.originalDocumentAction('document-save-as') },
          { value: '关闭原版文档  Ctrl+W', enabled: this.originalDocumentActive() && !this.originalDocumentOperationBusy(),
            action: () => this.originalDocumentAction('document-close') },
          { value: '内置合成无机显示示例（非科研数据）', enabled: this.applicationRuntime.initialized && !this.originalDocumentOperationBusy(),
            action: () => this.openSyntheticOriginalDocument() },
          { value: '导入设置…  Ctrl+Shift+O', enabled: this.applicationRuntime.initialized && !this.optionsTransferBusy,
            action: () => this.desktopAction('options-import') },
          { value: '导出设置…  Ctrl+Shift+S', enabled: this.applicationRuntime.initialized && !this.optionsTransferBusy,
            action: () => this.desktopAction('options-export') },
          { value: '退出 Olex2', action: () => this.exitOriginalApplication() }
        ])
      Button('视图').id('olex2-menu-view').width(desktopMenuButtonWidth('olex2-menu-view')).height(30)
        .onFocus(() => this.desktopMenuItemFocused('olex2-menu-view')).onBlur(() => { this.desktopMenuFocused = false; }).fontSize(12)
        .enabled(!this.originalTextEditorVisible && !this.pythonEditorVisible && !this.originalDocumentOperationBusy())
        .fontColor('#000000').backgroundColor('#F0F0F0')
        .bindMenu([
          { value: '工作台  Ctrl+1', action: () => this.desktopAction('panel-0') },
          { value: '基础检查  Ctrl+2', action: () => this.desktopAction('panel-1') },
          { value: '运行记录  Ctrl+3', action: () => this.desktopAction('panel-2') },
          { value: '重置视角  Ctrl+0', action: () => this.desktopAction('reset-view') },
          { value: this.sidebarVisible ? '隐藏侧栏  Ctrl+B' : '显示侧栏  Ctrl+B',
            action: () => this.desktopAction('toggle-sidebar') },
          { value: '命令输入  Ctrl+L', action: () => this.desktopAction('focus-command') },
          { value: '查看原始日志', action: () => this.desktopAction('original-log') },
          { value: 'Python 多行编辑器 / 打开脚本…', action: () => this.openPythonEditor() },
          { value: '查看 Python 状态与输出', action: () => this.showPythonOutput() }
        ])
      Button('结构').id('olex2-menu-structure').width(desktopMenuButtonWidth('olex2-menu-structure')).height(30)
        .onFocus(() => this.desktopMenuItemFocused('olex2-menu-structure')).onBlur(() => { this.desktopMenuFocused = false; }).fontSize(12)
        .enabled(!this.originalTextEditorVisible && !this.pythonEditorVisible && !this.originalDocumentOperationBusy())
        .fontColor('#000000').backgroundColor('#F0F0F0')
        .bindMenu([
          { value: '生成…', enabled: this.applicationRuntime.initialized,
            action: () => this.openOriginalGenerateDialog() },
          { value: '原子生长（grow）', enabled: this.originalSelectionEnabled(),
            action: () => { this.runOriginalSelectionCommand('grow'); this.originalSelectionMessage = this.commandPassed ? '已执行原版生长（grow）' : (this.commandError || '原版生长未通过'); } },
          { value: '晶格固定（fix c）', enabled: this.originalSelectionEnabled(),
            action: () => { this.runOriginalSelectionCommand('fix c'); this.originalSelectionMessage = this.commandPassed ? '已执行原版晶格固定（fix c）' : (this.commandError || '原版晶格固定未通过'); } },
          { value: '晶格释放（free cs）', enabled: this.originalSelectionEnabled(),
            action: () => { this.runOriginalSelectionCommand('free cs'); this.originalSelectionMessage = this.commandPassed ? '已执行原版晶格释放（free cs）' : (this.commandError || '原版晶格释放未通过'); } },
          { value: '选择组标签', enabled: this.originalSelectionEnabled(),
            action: () => this.createOriginalSelectionLabel() },
          { value: '片段隐藏所选', enabled: this.originalSelectionEnabled(),
            action: () => this.selectOriginalDocument('fragment-hide') },
          { value: '片段仅显示所选', enabled: this.originalSelectionEnabled(),
            action: () => this.selectOriginalDocument('fragment-show-only') },
          { value: '重置视角  Ctrl+0', action: () => this.desktopAction('reset-view') }
        ])
      Button('帮助').id('olex2-menu-help').width(desktopMenuButtonWidth('olex2-menu-help')).height(30)
        .onFocus(() => this.desktopMenuItemFocused('olex2-menu-help')).onBlur(() => { this.desktopMenuFocused = false; }).fontSize(12)
        .fontColor('#000000').backgroundColor('#F0F0F0')
        .bindMenu([
          { value: '快捷键帮助', action: () => this.desktopAction('shortcut-help') },
          { value: '关于 Olex2…', action: () => { this.aboutVisible = true; } },
          { value: '查看适配范围', action: () => this.showDesktopScope() },
          { value: '命令帮助', action: () => this.desktopAction('panel-2') },
          { value: '查看原始日志', action: () => this.desktopAction('original-log') }
        ])
      Button(this.sidebarVisible ? '收起侧栏' : '显示侧栏').id('olex2-sidebar-toggle').width(desktopMenuButtonWidth('olex2-sidebar-toggle')).height(30)
        .onFocus(() => this.desktopMenuItemFocused('olex2-sidebar-toggle')).onBlur(() => { this.desktopMenuFocused = false; }).fontSize(11)
        .fontColor('#000000').backgroundColor('#F0F0F0')
        .accessibilityDescription('显示或收起左侧导航和命令区，快捷键 Ctrl+B')
        .onClick(() => this.desktopAction('toggle-sidebar'))
      }.height(30).onKeyEvent((event: KeyEvent): boolean => this.desktopMenuKey(event))
      }.id('olex2-desktop-menu-scroll').width('100%').height(38)
        .padding({ left: 8, right: 8, top: 4, bottom: 4 }).scrollable(ScrollDirection.Horizontal)
        .scrollBar(BarState.Auto).align(Alignment.Start).clip(true)
        .onAreaChange((_oldArea: Area, area: Area) => this.measureDesktopMenu(area))
        .accessibilityDescription('F10 定位菜单；左右方向键切换，Home / End 首尾，Esc 返回当前工作区。')
      if (this.originalDocumentOperationBusy()) {
        Text('正在处理原版文档 / 选择…').fontSize(10).fontColor('#788C80').width('100%').maxLines(1)
          .padding({ left: 8, right: 8, bottom: 4 })
      } else if (this.optionsTransferBusy) {
        Text('正在处理设置文件…').fontSize(10).fontColor('#788C80').width('100%').maxLines(1)
          .padding({ left: 8, right: 8, bottom: 4 })
      }
    }.id('olex2-desktop-menu').width('100%').backgroundColor('#F0F0F0')
    if (this.shortcutHelpVisible) {
      Row({ space: 8 }) {
        Scroll() {
        Text('F10 定位顶部菜单；左右方向键切换，Home / End 首尾，Esc 返回工作区。Ctrl+O 打开文档；Ctrl+E 编辑；Ctrl+S 保存（首次选择文件）；文件菜单可另存为；Ctrl+W 关闭（保留未保存提醒）；Ctrl+Tab / Ctrl+Shift+Tab 切换文档；Ctrl+Shift+O / S 导入 / 导出设置。文本区 Ctrl+Z / Y 撤销 / 重做，Ctrl+F / H 查找替换，F3 / Shift+F3 前后查找，Ctrl+G 跳转到行。Ctrl+A 和剪贴板仍由当前输入框处理。')
          .width('100%').fontSize(11).fontColor('#516A5A')
        }.id('olex2-shortcut-help-scroll').layoutWeight(1).height('100%')
          .scrollable(ScrollDirection.Vertical).scrollBar(BarState.On).clip(true)
        Button('关闭').id('olex2-shortcut-help-close').height(28).fontSize(11).onClick(() => { this.shortcutHelpVisible = false; })
      }.id('olex2-shortcut-help').height(desktopHelpHeight(this.desktopHeight)).padding(10).width('100%').backgroundColor('#EAF2EC').borderRadius(8)
    }
  }

  private viewportKey(event: KeyEvent): boolean {
    if (this.pythonEditorVisible || this.originalTextEditorVisible || this.originalTextGuardBusy) return false;
    this.syncOriginalViewportInput();
    const key: string = event.keyCode === KeyCode.KEYCODE_A ? 'A' :
      event.keyCode === KeyCode.KEYCODE_ESCAPE ? 'Escape' : '';
    if (this.originalSelectionAvailable() && key.length > 0 && (event.type === KeyType.Down || event.type === KeyType.Up)) {
      const selection: OriginalViewportKeyResult = this.originalViewportInput.keyEvent(key, event.type === KeyType.Down,
        event.getModifierKeyState ? event.getModifierKeyState(['ctrl']) : false,
        event.getModifierKeyState ? event.getModifierKeyState(['alt']) : false,
        event.getModifierKeyState ? event.getModifierKeyState(['shift']) : false, this.desktopMetaKeys.size > 0);
      if (selection.handled) {
        event.stopPropagation();
        if (selection.action.length > 0) { this.keyEvents++; this.selectOriginalDocument(selection.action); }
        return true;
      }
    }
    if (!this.originalViewportFocused || !this.viewportInputAllowed() || event.type !== KeyType.Down) return false;
    if (event.getModifierKeyState &&
      (event.getModifierKeyState(['ctrl']) || event.getModifierKeyState(['alt']) ||
       this.desktopMetaKeys.size > 0)) return false;
    const code = event.keyCode;
    if (code >= KeyCode.KEYCODE_1 && code <= KeyCode.KEYCODE_4) {
      if (this.originalDocumentActive() || this.originalDocumentBusy) return false;
      this.keyEvents += 1;
      this.selectPrimitive(code - KeyCode.KEYCODE_1);
      return true;
    }
    if (code === KeyCode.KEYCODE_W) {
      if (this.originalDocumentActive() || this.originalDocumentBusy) return false;
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
    if (!this.viewportInputAllowed()) return;
    this.originalViewportInput.markDragged();
    const delta = event.getVerticalAxisValue();
    if (!Number.isFinite(delta) || delta === 0) return;
    this.distance = Math.max(2.2, Math.min(10, this.distance + Math.max(-0.5, Math.min(0.5, delta * 0.006))));
    this.axisEvents += 1; this.setView();
  }

  private rotateView(delta: number): void { if (!this.viewportInputAllowed()) return; this.yaw += delta; this.setView(); }
  private resetView(): void {
    if (!this.viewportInputAllowed()) return;
    this.originalViewportInput.markDragged();
    if (!this.originalDocumentActive()) {
      // Preserve the ordinary empty/demo reset; never invoke document native reset here.
      this.yaw = 32; this.pitch = -22; this.distance = 4.4; this.setView(); return;
    }
    const expected: OriginalDocumentEnvelope = this.originalDocument;
    try {
      if (!expected.core.loaded) throw new Error('原版文档尚未实际加载，未重置视角。');
      this.requireOriginalSessionNative(expected);
      const raw: string = nativeBridge.resetOriginalDocumentView();
      const document: OriginalDocumentEnvelope = parseOriginalDocument(raw);
      this.originalDocumentLastOperation = document; this.originalDocument = document; this.originalDocumentRaw = raw;
      this.graphics = nativeBridge.getGraphicsStatus();
      if (!this.sameOriginalSource(expected, document) || !this.originalDocumentSessions.matchesNative(document) ||
        !this.graphics.originalDocumentView || this.graphics.error.length > 0 ||
        document.core.selectionSequence !== expected.core.selectionSequence ||
        document.core.selectedObjectCount !== expected.core.selectedObjectCount) {
        throw new Error(document.error || document.core.error || this.graphics.error || '重置后来源、选择或实际呈现未确认。');
      }
      if (!Number.isFinite(this.graphics.yaw) || !Number.isFinite(this.graphics.pitch) || !Number.isFinite(this.graphics.distance) ||
        Math.abs(this.graphics.yaw - 32) > 0.0001 || Math.abs(this.graphics.pitch + 22) > 0.0001 ||
        Math.abs(this.graphics.distance - 4.4) > 0.0001) {
        throw new Error('原生重置后的实际相机参数未确认；不使用本地默认值冒充成功。');
      }
      this.yaw = this.graphics.yaw; this.pitch = this.graphics.pitch; this.distance = this.graphics.distance;
      this.originalSelectionMessage = '已按当前视口重置视角；文档来源与选择保持不变。';
    } catch (error) {
      this.originalSelectionMessage = '重置视角失败：' + ((error as Error).message || '原版视口未确认重置；可关闭文档。');
    } finally {
      this.refreshGraphics();
      this.syncOriginalViewportInput();
    }
  }
  private graphicsPassed(): boolean {
    return !this.originalDocumentActive() && this.graphics.surfaceReady && this.graphics.initialized && this.graphics.meshValidated && this.graphics.submissionValidated && this.graphics.coverageVerified && this.graphics.pixelVerified && this.graphics.geometryVerified && this.graphics.frames > 0 && this.graphics.error.length === 0;
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
      Text('原版解析 · 宏命令 / app.* / pc.*').fontSize(10).fontColor('#A8C2B5').width('100%')
      Row({ space: 4 }) {
        TextInput({ text: this.commandText, placeholder: '英文原版命令，如 help / arad pers' }).id('olex2-command-input')
          .layoutWeight(1).height(32).fontSize(11).maxLength(2048).backgroundColor('#E7EEE9')
          .type(InputType.Normal).enableAutoFill(false).enterKeyType(EnterKeyType.Go)
          .onChange((value: string) => { this.commandText = value; })
          .onSubmit(() => { this.submitCommand(); })
        Button('↑').id('olex2-command-history-prev').padding(0).width(24).height(28).fontSize(12).backgroundColor('#355345')
          .onClick(() => { this.commandHistoryStep(true); })
        Button('↓').id('olex2-command-history-next').padding(0).width(24).height(28).fontSize(12).backgroundColor('#355345')
          .onClick(() => { this.commandHistoryStep(false); })
        Button('×').id('olex2-command-clear').padding(0).width(24).height(28).fontSize(13).backgroundColor('#355345')
          .onClick(() => { this.commandText = ''; })
      }.width('100%')
      Row({ space: 6 }) {
        Button('执行视图').id('olex2-command-run').layoutWeight(1).padding({ left: 4, right: 4 }).height(30).fontSize(11).backgroundColor('#42735A')
          .onClick(() => { this.executeDesktopCommand(); })
        Button('应用函数').id('olex2-application-function-run').layoutWeight(1).padding({ left: 4, right: 4 }).height(30).fontSize(11).backgroundColor('#42735A')
          .onClick(() => { this.executeApplicationFunction(); })
        Button('原版命令').id('olex2-original-command-run').layoutWeight(1).padding({ left: 4, right: 4 }).height(30).fontSize(11).backgroundColor('#42735A')
          .onClick(() => { this.executeOriginalCommand(); })
        Button('Python').id('olex2-python-run').layoutWeight(1).padding({ left: 3, right: 3 }).height(30).fontSize(11).backgroundColor('#42735A')
          .enabled(this.pythonRuntime.initialized).onClick(() => { this.executePythonScript(); })
      }.width('100%')
      Button('脚本编辑 / 打开 .py…').id('olex2-python-editor-open-workspace').width('100%').height(28).fontSize(10).backgroundColor('#355345')
        .onClick(() => this.openPythonEditor())
      Button('命令帮助').id('olex2-command-help').width('100%').height(28).fontSize(10).backgroundColor('#355345')
        .onClick(() => { this.commandText = 'pc.help'; this.executeDesktopCommand(); })
      if (this.applicationRuntime.commandHints) {
        Text('arad pers / help / app.ConfigDir() / py print(1+1)').id('olex2-command-hint').fontSize(8).fontColor('#A2BDAD').width('100%')
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
        Text(this.originalDocumentActive() ? (this.originalDocumentViewPresented() ? '原版文档已呈现' : '原版文档待确认') : this.graphicsPassed() ? '像素读回通过' : this.graphics.error.length > 0 ? '绘图未通过' : this.activePanel === 0 ? '等待验证' : '绘图已暂停')
          .id('olex2-graphics-readiness')
          .fontSize(23).fontWeight(FontWeight.Medium).fontColor(this.graphicsPassed() ? '#24674A' : '#37443D').width('100%')
        Text(this.graphics.version.length > 0 ? this.graphics.version : '结果由实际 EGL / GLES 状态返回')
          .fontSize(11).fontColor('#8A938E').width('100%').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
      }.padding(18).alignItems(HorizontalAlign.Start).layoutWeight(1)
        .backgroundColor('#FFFFFF').borderRadius(14).border({ width: 1, color: '#E3E9E4' })
      Column({ space: 7 }) {
        Text('移植完成度').fontSize(12).fontColor('#738078').width('100%')
        Text('基础验证阶段').fontSize(23).fontWeight(FontWeight.Medium).fontColor('#37443D').width('100%')
        Text('原版界面与插件未完整适配；Python 已有限接入').fontSize(11).fontColor('#8A938E').width('100%')
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
  private OriginalSelectionContextMenu() {
    Menu() {
      MenuItem({ content: '清除选择（视口 Escape）' }).id('olex2-original-selection-context-clear')
        .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('clear-selection'))
      MenuItem({ content: '全选（视口 Ctrl+A）' }).id('olex2-original-selection-context-all')
        .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('select-all'))
      MenuItem({ content: '反选' }).id('olex2-original-selection-context-invert')
        .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('invert-selection'))
    }
  }

  @Builder
  private OriginalSelectionToolbar() {
    Column({ space: 5 }) {
      Row({ space: 5 }) {
        Text(this.originalDocument.core.selectedObjectCount === undefined ? '选中数待确认' :
          `选中 ${this.originalDocument.core.selectedObjectCount}`)
          .id('olex2-original-selection-count').fontSize(11).fontColor('#355443').layoutWeight(1)
        Button('清除').id('olex2-original-selection-clear').height(27).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('clear-selection'))
        Button('全选').id('olex2-original-selection-all').height(27).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('select-all'))
        Button('反选').id('olex2-original-selection-invert').height(27).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('invert-selection'))
        Button('晶胞').id('olex2-original-toggle-cell').height(27).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('toggle-cell'))
        Button('基矢').id('olex2-original-toggle-basis').height(27).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('toggle-basis'))
      }.width('100%')
      Row({ space: 4 }) {
        ForEach([0, 1, 2], (mode: number) => {
          Button(mode === 0 ? '替换' : mode === 1 ? '追加' : '切换')
            .id(`olex2-original-selection-mode-${mode}`).height(25).fontSize(10)
            .fontColor(this.originalSelectionMode === mode ? '#FFFFFF' : '#355443')
            .backgroundColor(this.originalSelectionMode === mode ? '#397654' : '#EDF3EF')
            .enabled(this.originalSelectionEnabled()).onClick(() => { this.originalSelectionMode = mode; })
        }, (mode: number) => `selection-mode-${mode}`)
        Button('包围框').id('olex2-original-toggle-frame').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('toggle-frame'))
        Button('模型居中').id('olex2-original-center-model').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('center-model'))
        Button('显示全部').id('olex2-original-show-all').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('show-all'))
        Button('视向').id('olex2-original-view-along').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .bindMenu([
            { value: '沿 100 方向查看', action: () => this.selectOriginalDocument('view-along-100') },
            { value: '沿 010 方向查看', action: () => this.selectOriginalDocument('view-along-010') },
            { value: '沿 001 方向查看', action: () => this.selectOriginalDocument('view-along-001') },
            { value: '沿 110 方向查看', action: () => this.selectOriginalDocument('view-along-110') },
            { value: '沿 101 方向查看', action: () => this.selectOriginalDocument('view-along-101') },
            { value: '沿 011 方向查看', action: () => this.selectOriginalDocument('view-along-011') },
            { value: '沿 111 方向查看', action: () => this.selectOriginalDocument('view-along-111') },
            { value: '沿所选键/平面查看', action: () => this.selectOriginalDocument('view-along-selection') }
          ])
        Button('隐藏选中').id('olex2-original-hide-selection').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .onClick(() => this.selectOriginalDocument('hide-selection'))
        Button('信息').id('olex2-original-atom-info').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .onClick(() => this.showOriginalAtomInfo())
        Text(this.originalViewportFocused ? '视口焦点' : '点击视口聚焦')
          .id('olex2-original-viewport-focus').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('片段').id('olex2-original-fragment-actions').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '隐藏该片段', action: () => this.selectOriginalDocument('fragment-hide') },
            { value: '仅显示该片段', action: () => this.selectOriginalDocument('fragment-show-only') },
            { value: '选中片段原子', action: () => this.selectOriginalDocument('fragment-select-atoms') },
            { value: '选中片段键', action: () => this.selectOriginalDocument('fragment-select-bonds') },
            { value: '选中整个片段', action: () => this.selectOriginalDocument('fragment-select') }
          ])
        Text('由当前选中原子/键确定片段；仅显示会重新居中').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('原子类型').id('olex2-original-atom-type').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: 'C', action: () => this.runOriginalSelectionCommand('name sel C') },
            { value: 'N', action: () => this.runOriginalSelectionCommand('name sel N') },
            { value: 'O', action: () => this.runOriginalSelectionCommand('name sel O') },
            { value: 'F', action: () => this.runOriginalSelectionCommand('name sel F') },
            { value: 'H', action: () => this.runOriginalSelectionCommand('name sel H') },
            { value: 'S', action: () => this.runOriginalSelectionCommand('name sel S') },
            { value: '更多…', action: () => this.openOriginalPeriodicTable() }
          ])
        Text('原版 Name 命令 · 修改当前选择的元素类型').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('占有率').id('olex2-original-occupancy').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '1', action: () => this.runOriginalSelectionCommand('fix occu 1 sel') },
            { value: '3/4', action: () => this.runOriginalSelectionCommand('fix occu 0.75 sel') },
            { value: '1/2', action: () => this.runOriginalSelectionCommand('fix occu 0.5 sel') },
            { value: '1/3', action: () => this.runOriginalSelectionCommand('fix occu 0.333333 sel') },
            { value: '1/4', action: () => this.runOriginalSelectionCommand('fix occu 0.25 sel') },
            { value: '固定', action: () => this.runOriginalSelectionCommand('fix occu sel') },
            { value: '自由', action: () => this.runOriginalSelectionCommand('free occu sel') }
          ])
        Text('原版 Fix/Free occu · 作用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('Uiso').id('olex2-original-uiso').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '0.02', action: () => this.runOriginalSelectionCommand('fix uiso 0.02 sel') },
            { value: '0.03', action: () => this.runOriginalSelectionCommand('fix uiso 0.03 sel') },
            { value: '0.05', action: () => this.runOriginalSelectionCommand('fix uiso 0.05 sel') },
            { value: '自由', action: () => this.runOriginalSelectionCommand('free uiso sel') }
          ])
        Text('原版 Fix/Free uiso · 绝对值作用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('连接').id('olex2-original-connectivity').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '0', action: () => this.runOriginalSelectionCommand('conn 0') },
            { value: '1', action: () => this.runOriginalSelectionCommand('conn 1') },
            { value: '2', action: () => this.runOriginalSelectionCommand('conn 2') },
            { value: '3', action: () => this.runOriginalSelectionCommand('conn 3') },
            { value: '4', action: () => this.runOriginalSelectionCommand('conn 4') },
            { value: '12', action: () => this.runOriginalSelectionCommand('conn 12') },
            { value: '24', action: () => this.runOriginalSelectionCommand('conn 24') }
          ])
        Text('原版 Conn · 最大成键数作用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('多面体').id('olex2-original-polyhedra').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '无', action: () => this.runOriginalSelectionCommand('poly none') },
            { value: '自动', action: () => this.runOriginalSelectionCommand('poly auto') },
            { value: '常规', action: () => this.runOriginalSelectionCommand('poly regular') },
            { value: '金字塔', action: () => this.runOriginalSelectionCommand('poly pyramid') },
            { value: '双金字塔', action: () => this.runOriginalSelectionCommand('poly bipyramid') }
          ])
        Text('原版 Poly · 多面体显示作用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('材质…').id('olex2-original-material').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0 && !this.materialDialogOpen)
          .onClick(() => this.openOriginalMaterialDialog())
        Text('原版 Draw style 材质对话框 · 应用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('坏反射…').id('olex2-original-bad-reflections').height(25).fontSize(10)
          .enabled(this.originalDocumentActive() && !this.originalDocumentOperationBusy())
          .onClick(() => this.readOriginalBadReflections())
        Text('原版 BadReflectionsTable 的坏反射列表（原版 HTML 表的数据源）').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('Part').id('olex2-original-part').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0)
          .bindMenu([
            { value: '0', action: () => this.runOriginalPartCommand(0) },
            { value: '1', action: () => this.runOriginalPartCommand(1) },
            { value: '2', action: () => this.runOriginalPartCommand(2) },
            { value: '3', action: () => this.runOriginalPartCommand(3) },
            { value: '4', action: () => this.runOriginalPartCommand(4) }
          ])
        Text('原版 Part · 设置当前选择的 part 编号').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('场景…').id('olex2-original-scene').height(25).fontSize(10)
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy() && !this.sceneDialogOpen)
          .onClick(() => this.openOriginalSceneDialog())
        Text('原版场景设置对话框（光照/背景）').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('背景…').id('olex2-original-gradient').height(25).fontSize(10)
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy() && !this.gradientDialogOpen)
          .onClick(() => this.openOriginalGradientDialog())
        Text('原版背景渐变对话框').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('更新选项…').id('olex2-original-update-options').height(25).fontSize(10)
          .enabled(!this.updateOptionsDialogOpen)
          .onClick(() => this.openOriginalUpdateOptionsDialog())
        Text('原版更新设置对话框（取消不改变设置）').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('球棍').id('olex2-original-style-pers').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.runOriginalDrawingStyle('pers', '球棍'))
        Button('椭球').id('olex2-original-style-telp').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.runOriginalDrawingStyle('telp', '椭球'))
        Button('空间填充').id('olex2-original-style-sfil').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.runOriginalDrawingStyle('sfil', '空间填充'))
        Button('棍状').id('olex2-original-style-bond').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.runOriginalDrawingStyle('bond', '棍状'))
        Button('各向同性').id('olex2-original-style-isot').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.runOriginalDrawingStyle('isot', '各向同性'))
        Text('原版绘图样式').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('编组').id('olex2-original-collectivise').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('collectivise'); this.originalSelectionMessage = this.commandPassed ? '已执行原版编组（collectivise）' : (this.commandError || '原版编组未通过'); })
        Button('解组').id('olex2-original-individualise').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('individualise'); this.originalSelectionMessage = this.commandPassed ? '已执行原版解组（individualise）' : (this.commandError || '原版解组未通过'); })
        Button('生长').id('olex2-original-atom-grow').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('grow'); this.originalSelectionMessage = this.commandPassed ? '已执行原版生长（grow）' : (this.commandError || '原版生长未通过'); })
        Button('平面激活').id('olex2-original-plane-activate').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.selectOriginalDocument('view-along-selection'); })
        Button('探索').id('olex2-original-atom-explore').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('mode grow -v=3.5'); this.originalSelectionMessage = this.commandPassed ? '已进入原版探索环境（mode grow -v=3.5）' : (this.commandError || '原版探索环境未通过'); })
        Button('选环').id('olex2-original-sel-rings').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('sel ring'); this.originalSelectionMessage = this.commandPassed ? '已执行原版选环（sel ring）' : (this.commandError || '原版选环未通过'); })
        Button('固定').id('olex2-original-lattice-fix').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('fix c'); this.originalSelectionMessage = this.commandPassed ? '已执行原版晶格固定（fix c）' : (this.commandError || '原版晶格固定未通过'); })
        Button('释放').id('olex2-original-lattice-free').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.runOriginalSelectionCommand('free cs'); this.originalSelectionMessage = this.commandPassed ? '已执行原版晶格释放（free cs）' : (this.commandError || '原版晶格释放未通过'); })
        TextInput({ text: this.originalBondRadiusText, placeholder: '键半径' }).id('olex2-bond-radius-input')
          .width(70).height(25).fontSize(10).backgroundColor('#E7EEE9').type(InputType.Normal)
          .onChange((value: string) => { this.originalBondRadiusText = value; })
        Button('选择组标签').id('olex2-original-selection-label').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => this.createOriginalSelectionLabel())
        Button('键长…').id('olex2-original-bond-length-dialog').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && !this.bondLengthDialogOpen)
          .onClick(() => this.openOriginalBondLengthDialog())
        Button('应用键半径').id('olex2-bond-radius-apply').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .onClick(() => { this.applyOriginalBondRadius(); })
        Button('图元…').id('olex2-original-primitive').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled() && this.originalDocument.core.selectedObjectCount !== undefined &&
            this.originalDocument.core.selectedObjectCount > 0 && !this.primitiveDialogOpen)
          .onClick(() => this.openOriginalPrimitiveDialog())
        Text('原版图元显示对话框 · 应用于当前选择').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      if (this.originalRecentLabels.length > 0) {
        Row({ space: 5 }) {
          ForEach(this.originalRecentLabels, (label: string, index: number) => {
            Button('最近 ' + label).id('olex2-recent-' + index).height(25).fontSize(10)
              .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
              .onClick(() => { this.openOriginalRecent(index); })
          }, (label: string, index: number) => label + '-' + index)
          Text('最近打开的文档').fontSize(10).fontColor('#758579').layoutWeight(1)
        }.width('100%')
      }
      Row({ space: 5 }) {
        Button('切换网格').id('olex2-original-toggle-grid').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled()).onClick(() => this.selectOriginalDocument('toggle-grid'))
        Text('需先载入晶体学网格数据').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('原子/键显示').id('olex2-original-atom-bond-visibility').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .bindMenu([
            { value: '切换整体结构显示', action: () => this.selectOriginalDocument('toggle-structure') },
            { value: '切换标签显示', action: () => this.selectOriginalDocument('toggle-labels') },
            { value: '切换氢原子', action: () => this.selectOriginalDocument('toggle-hydrogens') },
            { value: '切换氢键', action: () => this.selectOriginalDocument('toggle-hydrogen-bonds') },
            { value: '切换 Q 峰', action: () => this.selectOriginalDocument('toggle-qpeaks') },
            { value: '切换 Q 峰键', action: () => this.selectOriginalDocument('toggle-qpeak-bonds') }
          ])
        Text('保留原版可见性与连接重算行为').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Row({ space: 5 }) {
        Button('绘图质量').id('olex2-original-drawing-quality').height(25).fontSize(10)
          .enabled(this.originalSelectionEnabled())
          .bindMenu([
            { value: '低（原版 1 档）', action: () => this.selectOriginalDocument('quality-1') },
            { value: '中（原版 2 档）', action: () => this.selectOriginalDocument('quality-2') },
            { value: '高（原版 3 档）', action: () => this.selectOriginalDocument('quality-3') }
          ])
        Text('原版 TGXApp 图元细分质量').fontSize(10).fontColor('#758579').layoutWeight(1)
      }.width('100%')
      Text(this.originalSelectionBusy ? '正在处理原版选择…' : this.originalSelectionMessage)
        .id('olex2-original-selection-result').fontSize(10).fontColor('#64796A').width('100%').maxLines(3)
      Text('单击选择 · 拖动旋转 · Shift 追加 / Ctrl 切换 · 仅视口焦点 Ctrl+A 全选 / Escape 清除')
        .fontSize(10).fontColor('#758579').width('100%').maxLines(2)
    }.id('olex2-original-selection-toolbar').width('100%')
  }

  @Builder
  private OriginalDocumentToolbar() {
    Column({ space: 5 }) {
      Row({ space: 5 }) {
        Button('打开文档…').id('olex2-original-document-open').height(28).fontSize(10)
          .fontColor('#FFFFFF').backgroundColor('#397654')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.transferOriginalDocument('import'))
        Button('文本编辑…').id('olex2-original-document-edit').height(28).fontSize(10)
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy())
          .onClick(() => this.openOriginalTextEditor())
        Button(this.originalDocumentSavedName().length > 0 ? '保存' : '保存…')
          .id('olex2-original-document-save').height(28).fontSize(10)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy())
          .onClick(() => this.transferOriginalDocument('save'))
        Button('另存为…').id('olex2-original-document-export').height(28).fontSize(10)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy())
          .onClick(() => this.transferOriginalDocument('export'))
        Button('关闭文档').id('olex2-original-document-close').height(28).fontSize(10)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .enabled(this.originalDocumentActive() && !this.originalDocumentOperationBusy())
          .onClick(() => this.transferOriginalDocument('close'))
        Button('生成…').id('olex2-original-generate').height(28).fontSize(10)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .enabled(this.originalDocumentViewPresented() && !this.originalDocumentOperationBusy() && !this.generateDialogOpen)
          .onClick(() => this.openOriginalGenerateDialog())
        Blank()
      }.width('100%')
      Row({ space: 6 }) {
        Button('合成无机显示示例').id('olex2-original-document-synthetic').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openSyntheticOriginalDocument())
        Button('成键示例').id('olex2-original-document-bonded').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openBondedSyntheticOriginalDocument())
        Button('对称示例').id('olex2-original-document-symmetry').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openSymmetrySyntheticOriginalDocument())
        Button('XYZ 示例').id('olex2-original-document-xyz').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openXyzSyntheticOriginalDocument())
        Button('MOL 示例').id('olex2-original-document-mol').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openMolSyntheticOriginalDocument())
        Button('RES 示例').id('olex2-original-document-res').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openResSyntheticOriginalDocument())
        Button('PDB 示例').id('olex2-original-document-pdb').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openPdbSyntheticOriginalDocument())
        Button('SHELX 示例').id('olex2-original-document-shelx').height(26).fontSize(10)
          .fontColor('#536857').backgroundColor('#F2F5EC')
          .enabled(this.applicationRuntime.initialized && !this.originalDocumentOperationBusy())
          .onClick(() => this.openShelxSyntheticOriginalDocument())
        Text('非科研数据 · 仅平台显示').fontSize(10).fontColor('#7C8064').layoutWeight(1)
      }.width('100%')
      Text(this.originalDocumentBusy ? '正在处理原版文档；请等待文件操作完成…' : this.originalDocumentMessage)
        .id('olex2-original-document-transfer-status').fontSize(10).fontColor('#64796A').width('100%').maxLines(3)
      Text(this.originalDocumentStatusError.length > 0 ? this.originalDocumentStatusError :
        this.originalDocument.core.loaded ?
          `${this.originalDocument.core.name} · ${this.originalDocument.core.byteCount} 字节 · 修订 ${this.originalDocument.core.revision} · 原版帧 ${this.originalDocument.core.frames} · ${this.originalDocument.core.atomCount} 原子 / ${this.originalDocument.core.bondCount} 键 · ${this.originalDocument.core.renderObjects === undefined ? 0 : this.originalDocument.core.renderObjects} 渲染对象 / ${this.originalDocument.core.renderPrimitives === undefined ? 0 : this.originalDocument.core.renderPrimitives} 图元 · 集合 ${this.originalDocument.core.renderCollections === undefined ? 0 : this.originalDocument.core.renderCollections}${this.originalDocument.core.bondRadius === undefined ? '' : ' · 键半径 ' + this.originalDocument.core.bondRadius}${this.originalDocument.core.labelsVisible === undefined ? '' : this.originalDocument.core.labelsVisible ? ' · 标签显示' : ' · 标签隐藏'}${this.originalDocument.core.selectedParts === undefined || this.originalDocument.core.selectedParts.length === 0 ? '' : ' · Parts ' + this.originalDocument.core.selectedParts}${this.originalDocument.core.viewHash === undefined ? '' : ' · 视角 ' + this.originalDocument.core.viewHash}` :
          this.originalDocumentActive() ? '原版视图状态待确认；不沿用演示图元验证资格。' : '当前未载入原版文档；下方为原有平台演示图元。')
        .id('olex2-original-document-status').fontSize(10).fontColor('#758579').width('100%').maxLines(2)
      if (this.originalDocumentActive()) { this.OriginalSelectionToolbar() }
      if (this.originalDocument.core.error.length > 0 || this.originalDocument.error.length > 0) {
        Text(this.originalDocument.error || this.originalDocument.core.error)
          .id('olex2-original-document-native-error').fontSize(10).fontColor('#AD6C4E').width('100%').maxLines(3)
      }
    }.width('100%').padding({ left: 15, right: 15, top: 5, bottom: 7 })
  }

  private currentViewportLayout(): ViewportLayoutBudget {
    return viewportLayoutBudget(this.desktopHeight, this.viewportPanelHeight, this.viewportToolsExpanded);
  }

  @Builder
  private ViewportDiagnostics() {
    Column({ space: 6 }) {
      Text(this.originalDocumentActive() ? '原 TGlRenderer · 单击选择 / 拖动旋转 · 方向键 / 滚轮' : '1–4 切换图元 · W 线框 · R 重置 · 方向键 / 滚轮')
        .fontSize(11).fontColor('#89958E').width('100%').maxLines(2)
      Row({ space: 4 }) {
        Button(this.fontMetricsReady ? (this.originalFontProbe.length === 0 ? '测试原版字体' : '再次测试字体') : '字体参数未就绪')
          .id('olex2-original-font-probe').fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .accessibilityDescription(this.fontMetricsReady ? '使用当前窗口实际屏幕DPI' : this.fontMetricsError)
          .enabled(this.graphicsPassed() && this.fontMetricsReady && !this.originalDocumentActive() && !this.originalDocumentOperationBusy()).onClick(() => {
            this.originalFontProbe = nativeBridge.runOriginalFontProbe();
            this.refreshGraphics();
          })
        Button(this.originalTextureLoadProbe.length === 0 ? '加载原版纹理' : '再次加载纹理')
          .id('olex2-original-texture-load-probe').fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed() && !this.originalDocumentActive() && !this.originalDocumentOperationBusy()).onClick(() => {
            this.originalTextureLoadProbe = nativeBridge.runOriginalTextureLoadProbe();
            this.originalLog = nativeBridge.getOriginalLogs();
            this.refreshGraphics();
          })
        Button(this.originalProbeLabel()).id('olex2-original-renderer-probe')
          .fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed() && !this.originalDocumentActive() && !this.originalDocumentOperationBusy()).onClick(() => {
            this.originalRendererProbe = nativeBridge.runOriginalRendererProbe();
            this.refreshGraphics();
          })
      }.width('100%')
    }.width('100%').padding({ left: 10, right: 10, top: 4, bottom: 6 })
  }

  @Builder
  private Viewport() {
    Column({ space: 0 }) {
      // Original MDI child caption: the active document name with its close control.
      Row({ space: 6 }) {
        Text(this.originalDocumentActive()
          ? (this.originalDocument.core.name && this.originalDocument.core.name.length > 0
            ? this.originalDocument.core.name + ' — Olex2' : '原版文档 — Olex2')
          : '上游几何适配视口')
          .id('olex2-mdi-caption').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).fontSize(13)
          .fontWeight(FontWeight.Medium).fontColor('#253D30').layoutWeight(1)
        Button('×').id('olex2-mdi-close').width(30).height(24).fontSize(13)
          .fontColor('#355443').backgroundColor('#E7EEE9')
          .enabled(this.originalDocumentActive() && !this.originalDocumentOperationBusy())
          .onClick(() => this.originalDocumentAction('document-close'))
        Button(this.viewportToolsExpanded ? '收起工具' : '工具 / 状态')
          .id('olex2-viewport-tools-toggle').height(28).fontSize(11)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .accessibilityDescription('展开只调整工具区高度；也可在工具区滚动访问所有文档和诊断操作')
          .onClick(() => { this.viewportToolsExpanded = !this.viewportToolsExpanded; })
      }.width('100%').height(32).padding({ left: 10, right: 10 }).backgroundColor('#E9F0EA')
        .border({ width: { bottom: 1 }, color: '#C9D8CD' })
      Text(this.originalDocumentActive() ? '每标签独立保留视角与选择；已确认的会话备份可明确恢复，不覆盖演示工作台设置' : this.stateSaveStatus === 'saved' ? (this.stateLoadStatus === 'restored' ? '已恢复上次视图 · 当前已保存' : '当前视图已保存') : this.stateSaveStatus === 'write-error' ? '视图保存失败' : '正在保存视图')
        .id('olex2-workspace-state').fontSize(10).fontColor('#74867A').width('100%').height(16).maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).padding({ left: 10, right: 10 })
      Scroll() {
        Column({ space: 0 }) {
          this.OriginalDocumentToolbar()
      Row({ space: 8 }) {
        Button('↶').enabled(this.viewportInputAllowed()).width(38).height(32).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.rotateView(-20))
        Button('↷').enabled(this.viewportInputAllowed()).width(38).height(32).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.rotateView(20))
        Button('重置视角').id('olex2-view-reset').enabled(this.viewportInputAllowed()).height(32).fontSize(11).fontColor('#3C5F4B').backgroundColor('#EDF3EF').onClick(() => this.resetView())
        Blank()
        NativeViewportZoom({ distance: this.distance, inputAllowed: this.viewportInputAllowed(),
          onChange: (value: number): void => { if (!this.viewportInputAllowed()) return; this.originalViewportInput.markDragged(); this.distance = value; this.setView(); } })
      }.width('100%').padding({ left: 15, right: 15, top: 10, bottom: 10 })
          this.ViewportDiagnostics()
      if (!this.originalDocumentActive()) {
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
      }
        }.width('100%').alignItems(HorizontalAlign.Start)
      }.id('olex2-viewport-tools-scroll').width('100%').height(this.currentViewportLayout().toolsHeight)
        .scrollable(ScrollDirection.Vertical).scrollBar(BarState.On).clip(true)
      Stack({ alignContent: Alignment.TopStart }) {
        XComponent({ id: 'olex2-surface', type: XComponentType.SURFACE, libraryname: 'olex2native' })
          .width('100%').height('100%')
          .onLoad(() => this.refreshGraphics())
          .hitTestBehavior(HitTestMode.None)
        // Focusable transparent input plane: native SURFACE owns pixels only.
        Button('').id('olex2-viewport-input').width('100%').height('100%')
          .backgroundColor(Color.Transparent).borderRadius(0).focusable(true).focusOnTouch(true).defaultFocus(true)
          .enabled(this.viewportInputAllowed())
          .bindContextMenu(this.OriginalViewportContextMenu, ResponseType.RightClick)
          // Area and local MouseEvent/TouchObject positions all use layout vp.
          // Native graphics.width/height are pixels and never enter this ratio.
          .onAreaChange((_oldArea: Area, area: Area) => {
            this.originalViewportInput.setViewportSizeVp(Number(area.width), Number(area.height));
          })
          .onFocus(() => { this.originalViewportFocused = true; this.originalViewportInput.setFocused(true); })
          .onBlur(() => { this.originalViewportFocused = false; this.originalViewportInput.setFocused(false); })
          .onClick(() => { if (this.viewportInputAllowed()) this.getUIContext().getFocusController().requestFocus('olex2-viewport-input'); })
          .onMouse((event: MouseEvent) => this.viewportMouse(event))
          .onTouch((event: TouchEvent) => this.viewportTouch(event))
          .onKeyEvent((event: KeyEvent) => this.viewportKey(event))
          .onAxisEvent((event: AxisEvent) => this.viewportAxis(event))
          .onHover((hovered: boolean) => {
            this.viewportHovered = hovered;
            if (!hovered) this.originalViewportInput.cancelPointer();
            this.saveRecord();
          })
          .bindContextMenu(this.OriginalSelectionContextMenu, ResponseType.RightClick)

        Text(this.originalDocumentActive() ? (this.originalDocumentViewPresented() ? ((this.originalDocumentSourceKind === 'synthetic-platform-display' || this.originalDocumentSourceKind === 'edited-synthetic-platform-display') ? '合成无机显示示例 · 非科研数据 · 原版显示' : '原版文档显示 · 非完整原界面') : '原版文档未确认呈现 · 见上方状态') : `上游${this.currentPrimitiveLabel()}图元 · ${this.graphics.wireframe ? '线框' : '实体'} · 非结构模型`).fontSize(10).fontColor('#B4CBC0')
          .padding({ left: 10, right: 10, top: 6, bottom: 6 }).backgroundColor('#90203027')
          .borderRadius(5).margin(12).hitTestBehavior(HitTestMode.None)
      }.width('100%').layoutWeight(1).constraintSize({ minHeight: 0 }).backgroundColor('#0E141A')
    }.id('olex2-viewport-panel').width('100%').height('100%').backgroundColor('#FFFFFF').borderRadius(14)
      .border({ width: 1, color: '#E3E9E4' }).clip(true)
      .onAreaChange((_oldArea: Area, area: Area) => {
        const height: number = Number(area.height);
        this.viewportPanelHeight = Number.isFinite(height) && height > 0 ? height : 0;
      })
  }

  build() {
    Column() {
      this.DesktopMenuBar()
      Row({ space: 8 }) {
        Text(`会话备份 · ${this.originalRecoveryState} · 已确认批次 ${this.originalRecoveryStatus.generation}`)
          .id('olex2-recovery-compact-status').fontSize(11).fontColor('#355443').layoutWeight(1)
        Button(this.originalRecoveryPanelVisible ? '收起备份详情' : '会话备份详情')
          .id('olex2-recovery-toggle').height(28).fontSize(11)
          .onClick(() => { this.originalRecoveryPanelVisible = !this.originalRecoveryPanelVisible; })
      }.width('100%').padding({ left: 12, right: 12, top: 4, bottom: 4 }).backgroundColor('#EDF4EE')
      if (this.originalDocumentTabs.length > 0) {
        OriginalDocumentTabs({ sessions: this.originalDocumentTabs, saveTargets: this.originalDocumentSaveTargetSummaries,
          busy: this.originalDocumentOperationBusy() || this.pythonEditorVisible || this.optionsTransferBusy || this.pythonTransferBusy,
          onActivate: (id: string) => this.activateDesktopDocument(id),
          onClose: (id: string) => { this.closeOriginalDocumentSession(id); } })
      }
      Stack() {
      this.DesktopWorkspace()
      if (this.originalTextEditorVisible) {
        OriginalDocumentTextEditor({ documentId: this.boundOriginalDocumentId,
          source: this.originalTextEditorSource, name: this.originalTextEditorState.name,
          savedName: this.originalDocumentSavedName(), savedByteCount: this.originalDocumentSavedByteCount(),
          busy: this.originalDocumentOperationBusy(), draftDirty: this.originalTextEditorState.draftDirty,
          documentDirty: this.originalTextEditorState.documentDirty, ready: this.originalTextEditorState.initialized,
          message: this.originalTextEditorMessage, onEdit: (text: string) => this.editOriginalText(text),
          canUndo: this.originalTextCanUndo, canRedo: this.originalTextCanRedo,
          goToLineVisible: this.originalTextGoToLineVisible, goToLineInput: this.originalTextGoToLineInput,
          findVisible: this.originalTextFindVisible, query: this.originalTextQuery, replacement: this.originalTextReplacement,
          selectionStart: this.originalTextSelectionStart, selectionEnd: this.originalTextSelectionEnd,
          selectionSequence: this.originalTextSelectionSequence, focusTarget: this.originalTextFocusTarget,
          focusSequence: this.originalTextFocusSequence,
          onUndo: () => this.originalTextHistoryAction('undo'), onRedo: () => this.originalTextHistoryAction('redo'),
          onShortcut: (event: KeyEvent): boolean => this.originalDocumentKey(event),
          onFindVisible: (visible: boolean) => this.showOriginalTextFind(visible), onFindNext: () => this.findNextOriginalText(),
          onFindPrevious: () => this.findPreviousOriginalText(),
          onGoToLineVisible: (visible: boolean) => this.showOriginalTextGoToLine(visible),
          onGoToLineInput: (text: string) => {
            if (!this.originalDocumentOperationBusy() && this.originalTextGoToLineVisible) this.originalTextGoToLineInput = text;
          },
          onGoToLine: () => this.goToOriginalTextLine(),
          onQuery: (text: string) => { this.originalTextQuery = text; this.syncOriginalDocumentSessions(); },
          onReplacement: (text: string) => { this.originalTextReplacement = text; this.syncOriginalDocumentSessions(); },
          onReplace: (all: boolean) => this.replaceOriginalText(all),
          onSelection: (start: number, end: number) => {
            if (Number.isInteger(start) && Number.isInteger(end) && start >= 0 && end >= start && end <= this.originalTextEditorSource.length) {
              this.originalTextSelectionStart = start; this.originalTextSelectionEnd = end; this.syncOriginalDocumentSessions();
            }
          },
          onFocusedControl: (control: string) => { this.originalTextFocusedControl = control; },
          onToolError: (message: string) => { this.originalTextEditorMessage = message; this.saveRecord(); },
          onApply: () => this.applyOriginalText(), onSave: () => { this.transferOriginalDocument('save'); },
          onSaveAs: () => { this.transferOriginalDocument('export'); },
          onRevert: () => this.revertOriginalText(),
          onCloseDocument: () => { this.transferOriginalDocument('close'); },
          onBack: () => { this.originalTextEditorVisible = false; this.originalTextFocusedControl = '';
            this.originalTextGoToLineVisible = false; this.originalTextGoToLineInput = '';
            this.syncOriginalViewportInput(); this.saveRecord(); } })
      }
      if (this.pythonEditorVisible) {
        PythonEditor({ source: $pythonEditorSource, dirty: $pythonEditorDirty,
          busy: this.pythonTransferBusy, runtimeReady: this.pythonRuntime.initialized,
          message: this.pythonEditorMessage, output: this.pythonExecution.output, error: this.pythonExecution.error,
          onOpen: () => this.confirmPythonReplacement(() => { this.transferPythonDocument('import'); }),
          onSave: () => { this.transferPythonDocument('export'); }, onNew: () => this.newPythonDocument(),
          onClose: () => { this.pythonEditorVisible = false; this.saveRecord(); }, onRun: () => this.executeEditedPython() })
      }
      // Recovery is an overlay, not a sibling that can reduce the native
      // surface to zero height in the ordinary initial floating window.
      if (this.originalRecoveryPending || this.originalRecoveryPanelVisible || this.originalRecoveryError.length > 0) {
        Column() {
          Scroll() {
            OriginalDocumentRecoveryPanel({ state: this.originalRecoveryState, documentCount: this.originalRecoveryStatus.documentCount,
              generation: this.originalRecoveryStatus.generation, savedAt: this.originalRecoveryStatus.savedAt,
              detail: this.originalRecoveryMessage + (this.originalRecoveryPending && !this.originalRecoverySurfaceReady() ? '\n绘图窗口正在准备；就绪后才能明确恢复。' : '') +
                (this.originalRecoveryStatus.warning.length > 0 ? '\n' + this.originalRecoveryStatus.warning : ''),
              error: this.originalRecoveryError, busy: this.originalRecoveryBusy || this.originalDocumentBusy || this.originalTextGuardBusy,
              canRestore: this.originalRecoveryPending && this.originalRecoveryCanRestore && this.originalRecoverySurfaceReady(),
              canDiscard: this.originalRecoveryPending,
              canCheckpoint: this.originalRecoveryLoaded && !this.originalRecoveryPending && !this.originalDocumentOperationBusy() &&
                !this.optionsTransferBusy && !this.pythonTransferBusy,
              onRestore: () => this.restoreOriginalDocumentRecovery(),
              onDiscard: () => this.confirmDiscardOriginalDocumentRecovery(),
              onCheckpoint: () => this.flushOriginalDocumentRecovery(true) })
          }.id('olex2-recovery-scroll').width('100%').height('100%').constraintSize({ maxWidth: 680 })
        }.id('olex2-recovery-overlay').width('100%').height('100%').padding(12)
          .alignItems(HorizontalAlign.Center).backgroundColor('#F1F6F1')
      }
      }.width('100%').layoutWeight(1)
      if (this.aboutVisible) {
        Column({ space: 10 }) {
          Text('Olex2').fontSize(24).fontWeight(FontWeight.Bold).fontColor('#000000')
          Text('Harmony PC adaptation 0.22.62').fontSize(12).fontColor('#333333')
          Text('Original project: Olex2 (OlexSys) — GPL-2.0;\noriginal command library, renderer and dialogs are reused.')
            .fontSize(11).fontColor('#333333').textAlign(TextAlign.Center)
          Button('关闭').id('olex2-about-close').height(30).fontSize(12)
            .fontColor('#000000').backgroundColor('#F0F0F0')
            .onClick(() => { this.aboutVisible = false; })
        }.id('olex2-about-overlay').width(420).padding(18).backgroundColor('#FFFFFF')
          .border({ width: 1, color: '#C0C0C0' }).borderRadius(6)
          .position({ x: '40%', y: '35%' })
      }
      // Original command line (TCmdLine): macro entry above the status bar.
      Row({ space: 6 }) {
        Text('Olex2>').id('olex2-cmdline-prompt').fontSize(11).fontColor('#355443')
        TextInput({ text: this.cmdLineText, placeholder: '输入原版命令 / 宏…' }).id('olex2-cmdline-input')
          .layoutWeight(1).height(26).fontSize(11).backgroundColor('#FFFFFF').borderRadius(4)
          .onChange((value: string) => { this.cmdLineText = value; })
        Button('执行').id('olex2-cmdline-run').height(26).fontSize(11)
          .fontColor('#000000').backgroundColor('#F0F0F0')
          .onClick(() => { this.commandText = this.cmdLineText; this.executeOriginalCommand(); })
        Text(this.commandError.length > 0 ? this.commandError : this.commandMessage)
          .id('olex2-cmdline-result').fontSize(10)
          .fontColor(this.commandError.length > 0 ? '#A33A2B' : '#5E7367').maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis }).width(360)
      }.id('olex2-cmdline-bar').width('100%').height(34).padding({ left: 12, right: 12 })
        .backgroundColor('#F0F0F0').border({ width: { top: 1 }, color: '#C0C0C0' })
      // Original Olex2 status bar: welcome text plus the current document state.
      Row({ space: 12 }) {
        Text('Welcome to OLEX2!').id('olex2-status-welcome').fontSize(11).fontColor('#000000').layoutWeight(1)
        Text(this.originalDocumentViewPresented() ? '原版文档已呈现' : '无已呈现原版文档')
          .id('olex2-status-document').fontSize(11).fontColor('#5E7367')
        Text('Olex2 0.22.62').id('olex2-status-version').fontSize(11).fontColor('#5E7367')
      }.id('olex2-status-bar').width('100%').height(24).padding({ left: 12, right: 12 })
        .backgroundColor('#F0F0F0').border({ width: { top: 1 }, color: '#C0C0C0' })
    }.width('100%').height('100%')
      .onKeyEvent((event: KeyEvent): boolean => this.originalDocumentKey(event))
      .onBlur(() => { this.originalDocumentShortcutLatch.reset(); this.desktopDocumentNavigationLatch.reset(); this.desktopMetaKeys.clear(); })
  }

  private exitOriginalApplication(): void {
    try {
      const context: common.UIAbilityContext = getContext(this) as common.UIAbilityContext;
      context.terminateSelf();
    } catch (error) {
      this.originalSelectionMessage = (error as Error).message || '退出原版应用失败';
    }
  }

  @Builder
  private OriginalViewportContextMenu() {
    Menu() {
      MenuItem({ content: '原子信息（Info）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.showOriginalAtomInfo())
      MenuItem({ content: '选择组标签' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.createOriginalSelectionLabel())
      MenuItem({ content: '键长…（原版线长对话框）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.openOriginalBondLengthDialog())
      MenuItem({ content: '键半径（应用当前值）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.applyOriginalBondRadius())
      MenuItem({ content: '球棍（pers）' }).onClick(() => this.runOriginalDrawingStyle('pers', '球棍'))
      MenuItem({ content: '椭球（telp）' }).onClick(() => this.runOriginalDrawingStyle('telp', '椭球'))
      MenuItem({ content: '空间填充（sfil）' }).onClick(() => this.runOriginalDrawingStyle('sfil', '空间填充'))
      MenuItem({ content: '棍状（bond）' }).onClick(() => this.runOriginalDrawingStyle('bond', '棍状'))
      MenuItem({ content: '各向同性（isot）' }).onClick(() => this.runOriginalDrawingStyle('isot', '各向同性'))
      MenuItem({ content: '标签显示/隐藏' }).onClick(() => this.selectOriginalDocument('toggle-labels'))
      MenuItem({ content: '氢原子显示/隐藏' }).onClick(() => this.selectOriginalDocument('toggle-hydrogens'))
      MenuItem({ content: 'Q峰显示/隐藏' }).onClick(() => this.selectOriginalDocument('toggle-qpeaks'))
      MenuItem({ content: '绘图质量：低' }).onClick(() => this.selectOriginalDocument('quality-1'))
      MenuItem({ content: '绘图质量：中' }).onClick(() => this.selectOriginalDocument('quality-2'))
      MenuItem({ content: '绘图质量：高' }).onClick(() => this.selectOriginalDocument('quality-3'))
      MenuItem({ content: '全选' }).onClick(() => this.selectOriginalDocument('select-all'))
      MenuItem({ content: '清除选择' }).onClick(() => this.selectOriginalDocument('clear-selection'))
      MenuItem({ content: '反选' }).onClick(() => this.selectOriginalDocument('invert-selection'))
      MenuItem({ content: '编组' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.runOriginalSelectionCommand('collectivise'))
      MenuItem({ content: '解组' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.runOriginalSelectionCommand('individualise'))
      MenuItem({ content: '片段隐藏' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.selectOriginalDocument('fragment-hide'))
      MenuItem({ content: '片段仅显示' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.selectOriginalDocument('fragment-show-only'))
      MenuItem({ content: '原子生长（grow）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.runOriginalSelectionCommand('grow'))
      MenuItem({ content: '晶格固定（fix c）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.runOriginalSelectionCommand('fix c'))
      MenuItem({ content: '晶格释放（free cs）' }).enabled(this.originalSelectionEnabled())
        .onClick(() => this.runOriginalSelectionCommand('free cs'))
      MenuItem({ content: '重置视角（Ctrl+0）' }).onClick(() => this.desktopAction('reset-view'))
    }
  }

  @Builder
  private DesktopWorkspace() {
    Row({ space: 0 }) {
      if (this.sidebarVisible) {
      Column({ space: 0 }) {
        Column({ space: 8 }) {
          Text('Olex2').fontSize(compactDesktopSidebar(this.desktopHeight) ? 24 : 32)
            .fontWeight(FontWeight.Medium).fontColor('#FFFFFF').width('100%')
          Text('OLEX2 · CRYSTALLOGRAPHY').fontSize(10).fontColor('#90B6A1').width('100%')
        }.id('olex2-sidebar-brand').padding({ left: 20, right: 20,
          top: compactDesktopSidebar(this.desktopHeight) ? 10 : 30,
          bottom: compactDesktopSidebar(this.desktopHeight) ? 12 : 34 }).alignItems(HorizontalAlign.Start)
        Scroll(this.sidebarScroller) {
          Column({ space: 0 }) {
            Column({ space: 6 }) {
              this.NavItem(0, '◇', '适配工作台')
              this.NavItem(1, '✓', '基础代码检查')
              this.NavItem(2, '≡', '运行记录')
            }.padding({ left: 12, right: 12 })
            this.CommandEntry()
          }.id('olex2-sidebar-body').width('100%').padding({ bottom: 10 })
            .onAreaChange((_oldArea: Area, area: Area) => {
              const height: number = Number(area.height);
              this.sidebarContentHeight = Number.isFinite(height) && height > 0 ? height : 0;
              this.revealDesktopCommandInput();
            })
        }.id('olex2-sidebar-scroll').width('100%').layoutWeight(1)
          .scrollable(ScrollDirection.Vertical).scrollBar(BarState.On).clip(true)
          .onAreaChange((_oldArea: Area, area: Area) => {
            const height: number = Number(area.height);
            this.sidebarViewportHeight = Number.isFinite(height) && height > 0 ? height : 0;
            this.revealDesktopCommandInput();
          })
        Column({ space: 6 }) {
          Text(this.packageIdentityError.length > 0 ? this.packageIdentityError :
            this.versionName.length > 0 ? `开发验证版  /  ${this.versionName}` : '正在读取已安装版本')
            .id('olex2-installed-version').fontSize(11).fontColor('#9BB2A6').width('100%')
          Text('阶段开发版，尚非完整移植。').id('olex2-sidebar-scope-hint')
            .fontSize(11).fontColor('#8BA494').lineHeight(16).width('100%')
          Button('查看适配范围').id('olex2-sidebar-scope').width('100%').height(26).fontSize(11)
            .fontColor('#C3D6CB').backgroundColor('#294437').onClick(() => this.showDesktopScope())
        }.id('olex2-sidebar-footer').padding(12).width('100%').alignItems(HorizontalAlign.Start)
      }.id('olex2-sidebar').width(216).height('100%').backgroundColor('#192D24')
      }
      Column({ space: this.currentViewportLayout().workspaceGap }) {
        Row() {
          Column({ space: 6 }) {
            Text('鸿蒙 PC  /  原生集成').fontSize(11).fontColor('#7D8C82')
            Text(this.activePanel === 0 ? '适配工作台' : this.activePanel === 1 ? '基础代码检查' : '运行记录')
              .fontSize(26).fontWeight(FontWeight.Medium).fontColor('#263B2D')
          }.alignItems(HorizontalAlign.Start).layoutWeight(1)
          Text('开发验证版').fontSize(11).fontColor('#8B7757').backgroundColor('#F0EBE0')
            .padding({ left: 12, right: 12, top: 7, bottom: 7 }).borderRadius(7)
        }.width('100%').height(this.currentViewportLayout().compact ? 0 : 54)
          .visibility(this.currentViewportLayout().compact ? Visibility.None : Visibility.Visible)
        if (!this.currentViewportLayout().compact && !compactDesktopSummary(this.desktopWidth, this.sidebarVisible, this.desktopHeight)) {
          this.SummaryCards()
        } else {
          Text(this.checks.passed ? '基础代码已通过 · 可从视图菜单查看检查和记录' : '基础代码状态见检查面板')
            .id('olex2-compact-summary').fontSize(11).fontColor('#738078').width('100%')
            .visibility(this.currentViewportLayout().compact ? Visibility.None : Visibility.Visible)
        }
        if (this.activePanel === 0) {
          Row({ space: 16 }) {
            Column() { this.Viewport() }.layoutWeight(3).height('100%')
            if (showDesktopChecksPanel(this.desktopWidth, this.sidebarVisible, this.desktopHeight)) {
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
            }
          }.width('100%').layoutWeight(1)
        } else if (this.activePanel === 1) {
          Scroll() {
          Column({ space: 16 }) {
            Row() {
              Column({ space: 5 }) {
                Text('来自真实上游的基础实现').fontSize(17).fontWeight(FontWeight.Medium).fontColor('#2E4837')
                Text('不是预填的通过状态。每项结果均来自 C++ 执行返回。').fontSize(12).fontColor('#849389')
              }.layoutWeight(1).alignItems(HorizontalAlign.Start)
              Button('重新执行检查').height(36).fontSize(12).backgroundColor('#327550').onClick(() => this.runChecks())
            }.width('100%')
            this.CheckRows()
            Text('文件检查只在应用私有目录中独占创建并清理自己的临时文件，不覆盖已有内容。')
              .fontSize(12).fontColor('#849389').width('100%')
          }.padding(20).width('100%')
          }.id('olex2-checks-scroll').width('100%').layoutWeight(1).scrollable(ScrollDirection.Vertical).focusable(true)
            .scrollBar(BarState.On).clip(true).backgroundColor('#FFFFFF').borderRadius(14)
        } else {
          Scroll() {
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
              Text(this.originalLog.entryCount > 0 ? this.originalLogText() : '本次会话暂无原版日志')
                  .id('olex2-core-log-text').fontSize(12).fontFamily('monospace').fontColor('#385140').width('100%')
              Text('缓冲最多256条/64KiB；不自动恢复或重放。仅观察原TLog通知，不含直接流输出或级别。诊断摘要另存私有记录。')
                .fontSize(10).fontColor('#849389').width('100%').maxLines(3)
            } else {
              Text(this.lastRecord).id('olex2-platform-record-text').fontSize(12).fontFamily('monospace').fontColor('#385140').width('100%')
            }
            Text(this.recordError.length > 0 ? this.recordError : '记录保存于应用私有目录；fullPortComplete 始终反映当前未完成状态。')
              .fontSize(11).fontColor('#849389').width('100%')
          }.padding(20).width('100%')
          }.id('olex2-records-scroll').width('100%').layoutWeight(1).scrollable(ScrollDirection.Vertical).focusable(true)
            .scrollBar(BarState.On).clip(true).backgroundColor('#FFFFFF').borderRadius(14)
        }
        Text('范围说明：上游图元与提交循环已接入；显示适配为新实现，完整原版渲染器与功能尚未移植。')
          .fontSize(10).fontColor('#89958E').width('100%')
          .visibility(this.currentViewportLayout().compact ? Visibility.None : Visibility.Visible)
      }.padding(this.currentViewportLayout().workspacePadding).layoutWeight(1).height('100%').backgroundColor('#F4F7F3')
    }.width('100%').height('100%').visibility(this.pythonEditorVisible || this.originalTextEditorVisible ? Visibility.Hidden : Visibility.Visible)
      .onAreaChange((_oldArea: Area, area: Area) => {
        const width: number = Number(area.width);
        if (Number.isFinite(width) && width > 0) this.desktopWidth = width;
        const height: number = Number(area.height);
        if (Number.isFinite(height) && height > 0) this.desktopHeight = height;
      })
      .onKeyEvent((event: KeyEvent) => this.desktopKey(event))
      .onBlur(() => { this.desktopShortcutLatch.reset(); this.desktopMetaKeys.clear(); })
  }
}
