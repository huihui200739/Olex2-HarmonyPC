export interface OriginalLinePathGpuSample { name: string; x: number; y: number; expectedLit: boolean; passed: boolean; rgba: number[]; }
export interface OriginalLinePathGpuCase { kind: string; indexSha256: string; inputVertices: number; indexCount: number; actualLineWidth: number; actualBufferVerified: boolean; pixelsVerified: boolean; actualIndices: number[]; samples: OriginalLinePathGpuSample[]; }
export interface OriginalLinePathGpuProbe { verified: boolean; stateRestored: boolean; contextRestored: boolean; source: string; error: string; cases: OriginalLinePathGpuCase[]; }
export interface CheckResult { name: string; passed: boolean; detail: string; }
export interface UtilityReport { passed: boolean; checks: CheckResult[]; error: string; }
export interface GraphicsStatus {
  originalDocumentView?: boolean;
  originalDocumentFrames?: number;
  fontDisplay?: FontDisplayMetricsStatus;
  linePathProbe: OriginalLinePathGpuProbe;
  surfaceReady: boolean; initialized: boolean; pixelVerified: boolean; geometryVerified: boolean;
  coverageVerified: boolean; meshValidated: boolean; wireframe: boolean;
  submissionValidated: boolean; submittedVertices: number; submissionSource: string;
  originalTriangleVerified: boolean; originalTriangleVertices: number; originalTriangleNormalCommands: number; originalTriangleColorCommands: number; originalTriangleSource: string;
  originalLineVerified: boolean; originalLineBufferVerified: boolean; originalLineVertices: number; originalLineColorCommands: number; originalLineWidthQueries: number; originalLineWidthCommands: number; originalLineSource: string; originalLineIndexSha256: string;
  originalPolygonVerified: boolean; originalPolygonBatches: number; originalPolygonVertices: number; originalPolygonNormalCommands: number; originalPolygonColorCommands: number; originalPolygonSource: string;
  polygonProbeContextRestored: boolean; polygonProbeStateRestored: boolean; polygonProbeVerified: boolean; polygonProbeSource: string; polygonProbeInputVertices: number; polygonProbeTriangles: number; polygonProbeNormalCommands: number; polygonProbeColorCommands: number; polygonProbeSamples: number[][];
  basisMatrixVerified: boolean; matrixUniformVerified: boolean; basisMatrixSource: string;
  modelViewDepth: number; projectionDepth: number; matrixUniformMaxError: number;
  modelViewMatrix: number[]; mvpMatrix: number[]; normalMatrix: number[];
  meshKind: string; sourceBatches: number; discardedTailVertices: number; degenerateTrianglesSkipped: number; geometryColorSamples: number;
  materialPreset: number; materialRevision: number; materialFlags: number; materialSource: string; materialCanonical: string;
  ambientR: number; ambientG: number; ambientB: number; diffuseR: number; diffuseG: number; diffuseB: number;
  emissionR: number; emissionG: number; emissionB: number; materialVerified: boolean; materialCommandCount: number; materialCommandsSource: string;
  specularR: number; specularG: number; specularB: number; shininess: number;
  specularComparedPixels: number; specularChangedPixels: number; specularMaxError: number; specularMaxDelta: number; specularReferenceVerified: boolean; specularSamples: number[][];
  rowMeanScale: number; rowMinScale: number; rowMaxScale: number;
  colorPreset: number; colorRevision: number; colorPacked: number; colorSource: string;
  colorR: number; colorG: number; colorB: number; colorA: number; colorVerified: boolean;
  readbackR: number; readbackG: number; readbackB: number; readbackA: number;
  meshGeneration: number; meshRevision: number; meshVertices: number; meshTriangles: number; meshIndices: number; meshSource: string;
  width: number; height: number; frames: number; yaw: number; pitch: number; distance: number; version: string; renderer: string; error: string;
}
export interface DesktopCommandReport {
  passed: boolean; changed: boolean; applied: boolean; dispatched: number;
  text: string; error: string; source: string; graphics: GraphicsStatus;
}
export const executeDesktopCommand: (command: string) => DesktopCommandReport;
export const runUtilityChecks: (directory: string) => UtilityReport;
export const getGraphicsStatus: () => GraphicsStatus;
export const setView: (yaw: number, pitch: number, distance: number) => GraphicsStatus;
export const setMesh: (generation: number, wireframe: boolean) => GraphicsStatus;
export const setPrimitive: (kind: number) => GraphicsStatus;
export const setColorPreset: (preset: number) => GraphicsStatus;
export const setMaterialPreset: (preset: number) => GraphicsStatus;

export interface ApplicationRuntimeStatus {
  initialized: boolean; optionsRestored: boolean; licenseVerified: boolean; commandHints: boolean; lastSaveCommitted: boolean;
  originalFunctionCount: number; optionsCount: number; saveRevision: number;
  originalCoreInitialized: boolean; originalCoreEmptyFile: boolean;
  originalCoreMacroCount: number; originalCoreFunctionCount: number; originalCoreFileCount: number;
  baseDir: string; instanceDir: string; sharedDir: string; configDir: string;
  licenseSha256: string; optionsSha256: string; platform: string; error: string; source: string;
}
export interface ApplicationFunctionReport {
  passed: boolean; optionsChanged: boolean; input: string; text: string; error: string; source: string; runtime: ApplicationRuntimeStatus;
}
export const initializeApplicationRuntime: (filesDir: string, originalLicense: Uint8Array) => ApplicationRuntimeStatus;
export const getApplicationRuntimeStatus: () => ApplicationRuntimeStatus;
export const shutdownApplicationRuntime: () => void;
export const evaluateApplicationFunction: (expression: string) => ApplicationFunctionReport;
export interface OriginalCommandReport {
  passed: boolean; input: string; text: string; output: string; error: string; runtime: ApplicationRuntimeStatus;
}
// Executes a command through the replicated original application macro library
// plus the ported Harmony main-form actions. Output is the bounded original log
// text produced by the command; no separate macro parser is used.
export const executeOriginalCommand: (command: string) => OriginalCommandReport;
export interface OriginalGenerateDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  values?: number[];
  error: string;
}
// Original Structure -> Generate... dialog (native ArkUI dialog host). Open it,
// then poll until the modal result is available; on OK run the original pack
// command with the returned A/B/C ranges.
export const openOriginalGenerateDialog: () => { opened: boolean; error: string };
export const pollOriginalGenerateDialog: () => OriginalGenerateDialogReport;
export interface OriginalPeriodicTableReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  symbol?: string;
  error: string;
}
// Original periodic table (atom type "More..."): selecting an element ends the
// dialog and returns its symbol.
export const openOriginalPeriodicTable: () => { opened: boolean; error: string };
export const pollOriginalPeriodicTable: () => OriginalPeriodicTableReport;
export interface OriginalMaterialDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  applied?: boolean;
  sceneChanged?: boolean;
  signature?: string;
  error: string;
}
// Original material editor (Graphics -> Draw style...). On OK the material is
// applied to the current selection by the native dialog itself.
export const openOriginalMaterialDialog: () => { opened: boolean; error: string };
export const pollOriginalMaterialDialog: () => OriginalMaterialDialogReport;
export interface OriginalSceneDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  sceneChanged?: boolean;
  signature?: string;
  error: string;
}
// Original scene-properties dialog (Draw style settings). OK applies the light
// model and closes the dialog.
export const openOriginalSceneDialog: () => { opened: boolean; error: string };
export const pollOriginalSceneDialog: () => OriginalSceneDialogReport;
export interface OriginalGradientDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  applied?: boolean;
  uncertain?: boolean;
  error: string;
}
// Original background gradient dialog. OK applies the corner colours and closes.
export const openOriginalGradientDialog: () => { opened: boolean; error: string };
export const pollOriginalGradientDialog: () => OriginalGradientDialogReport;
export interface OriginalPrimitiveDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  applied?: boolean;
  mask?: number;
  level?: number;
  error: string;
}
// Original primitives dialog for the current selection; OK applies the mask.
export const openOriginalPrimitiveDialog: () => { opened: boolean; error: string };
export const pollOriginalPrimitiveDialog: () => OriginalPrimitiveDialogReport;
export interface OriginalUpdateOptionsDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  error: string;
}
// Original update options dialog; Cancel closes without changing settings.
export const openOriginalUpdateOptionsDialog: () => { opened: boolean; error: string };
export interface OriginalBondRadiusReport { passed: boolean; radius: number; error: string; }
// Sets the original bond radius style (TXBond::Settings) without commands.
export const setOriginalBondRadius: (radius: number) => OriginalBondRadiusReport;
export interface OriginalSelectionLabelReport { passed: boolean; objectCount: number; text: string; error: string; }
// Original ID_SelLabel: create a text label at the original selection centroid.
export const createOriginalSelectionLabel: (text: string) => OriginalSelectionLabelReport;
export interface OriginalBadReflectionsReport {
  passed: boolean;
  count: number;
  hklf: number;
  json: string;
  error: string;
}
export const readOriginalBadReflections: () => OriginalBadReflectionsReport;
export interface OriginalBondLengthDialogReport {
  state: 'none' | 'open' | 'done';
  accepted?: boolean;
  applied?: boolean;
  length?: number;
  error: string;
}
// Original TdlgEdit bond length dialog (prefilled with the selected bond length).
export const openOriginalBondLengthDialog: () => { opened: boolean; error: string };
export const pollOriginalBondLengthDialog: () => OriginalBondLengthDialogReport;
export const pollOriginalUpdateOptionsDialog: () => OriginalUpdateOptionsDialogReport;

export interface ApplicationOptionsTransferReport {
  passed: boolean; optionsChanged: boolean; importedOptions: number;
  data: Uint8Array; sha256: string; error: string; runtime: ApplicationRuntimeStatus;
}
export const exportApplicationOptions: () => ApplicationOptionsTransferReport;
export const importApplicationOptions: (data: Uint8Array) => ApplicationOptionsTransferReport;

export interface OriginalLogEntry { sequence: number; text: string; truncated: boolean; }
export interface OriginalLogSnapshot {
  attached: boolean; posted: number; dropped: number; truncated: number; captureFailures: number;
  clearCount: number; clearedEntries: number; entryCount: number; retainedBytes: number;
  textSha256: string; source: string; error: string; entries: OriginalLogEntry[];
}
export interface CoreCatalogReport {
  passed: boolean; exposedMacros: number; postedBefore: number; postedAfter: number;
  input: string; error: string; source: string;
}
export const getOriginalLogs: () => OriginalLogSnapshot;
export const clearOriginalLogs: () => OriginalLogSnapshot;
export const runCoreCatalog: (command: string) => CoreCatalogReport;

export const runOriginalRendererProbe: () => string;
export const runOriginalTextureLoadProbe: () => string;
export const runOriginalFontProbe: () => string;
export interface FontDisplayMetricsStatus {
  ready: boolean; displayId: number; revision: number; dpiX: number; dpiY: number;
  densityPixels: number; fixedDpiFallback: boolean;
}
export const setFontDisplayMetrics: (displayId: number, dpiX: number, dpiY: number, densityPixels: number) => FontDisplayMetricsStatus;
export const clearFontDisplayMetrics: () => FontDisplayMetricsStatus;
export const getFontDisplayMetrics: () => FontDisplayMetricsStatus;

export const runGxPlatformEvaluation: (filesDir: string) => string;

export interface PythonRuntimeStatus {
  prepared: boolean; initialized: boolean; originalBridgeImported: boolean; executions: number;
  version: string; home: string; stdlibSha256: string; error: string;
}
export interface PythonExecutionReport {
  passed: boolean; optionsChanged: boolean; output: string; error: string; runtime: PythonRuntimeStatus;
}
export const initializePythonRuntime: (stdlib: Uint8Array) => PythonRuntimeStatus;
export const getPythonRuntimeStatus: () => PythonRuntimeStatus;
export const executePythonScript: (source: string) => PythonExecutionReport;

// Private candidate38 ABI: JSON envelopes are parsed/validated by the document UI.
export const openOriginalDocument: (name: string, bytes: Uint8Array) => string;
// Existing original viewport only. x/y: finite normalized top-left [0,1].
// action: pick | clear-selection | select-all | invert-selection | toggle-cell | toggle-basis | toggle-frame | toggle-grid | show-all | center-model | hide-selection | quality-1/2/3 | toggle-hydrogens/hydrogen-bonds/qpeaks/qpeak-bonds/labels/structure | fragment-select/select-atoms/select-bonds/hide/show-only | view-along-100/010/001/110/101/011/111/selection.
// mode: integer 0 replace, 1 add, 2 toggle; all four arguments are required.
// Returns the actual document/core/presentation envelope (including failures).
export const interactOriginalDocument: (action: string, x: number, y: number, mode: number) => string;
// Redraws the retained original document without changing selection or camera.
// Returns the actual presentation envelope; used after original commands.
export const redrawOriginalDocument: () => string;
export const closeOriginalDocument: () => string;
export const getOriginalDocumentStatus: () => string;
export const exportOriginalDocument: () => Uint8Array;
// Exactly zero arguments. Fresh verified staged input, not canonical saver output.
// Throws Error if no real document is loaded or the source cannot be verified.
export const readOriginalDocumentSource: () => Uint8Array;

// Exactly zero arguments. Explicit loaded/owned main-viewport reset only.
// Uses the native ResetView fit at the actual current surface size; no demo fallback.
// Returns the real presentation envelope; throws on any reset/presentation failure.
export const resetOriginalDocumentView: () => string;

// Exactly zero arguments. Throws on any source/inventory/selection/basis or
// actual presentation failure; never returns a cached or synthetic snapshot.
// Strict schemaVersion=1: name,byteCount,sha256,yaw,pitch,distance,objectCount,
// inventorySha256,selectedObjectIndices. Capture does not change selectionSequence.
export const captureOriginalDocumentViewport: () => string;
// Exactly one bounded snapshot JSON string. Validates the entire source and
// ordered inventory before mutation; uses original TGlRenderer::Select/Draw.
// Success returns the document presentation envelope, selectionAction=restore-selection.
// Throws on failure; source revision is unchanged. No old revision proves ownership.
export const restoreOriginalDocumentViewport: (snapshot: string) => string;

// Original C++ label/slider hosted in a real ArkUI NodeContent, API12+.
// Opaque owner tokens are invalid after unmount; programmatic updates are silent.
export const mountNativeViewportZoom: (content: Object, tenths: number, enabled: boolean, onChange: (distance: number) => void) => number;
export const updateNativeViewportZoom: (owner: number, tenths: number, enabled: boolean) => void;
export const unmountNativeViewportZoom: (owner: number) => void;
