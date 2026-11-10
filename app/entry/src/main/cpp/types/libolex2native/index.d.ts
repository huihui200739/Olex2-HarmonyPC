export interface OriginalLinePathGpuSample { name: string; x: number; y: number; expectedLit: boolean; passed: boolean; rgba: number[]; }
export interface OriginalLinePathGpuCase { kind: string; indexSha256: string; inputVertices: number; indexCount: number; actualLineWidth: number; actualBufferVerified: boolean; pixelsVerified: boolean; actualIndices: number[]; samples: OriginalLinePathGpuSample[]; }
export interface OriginalLinePathGpuProbe { verified: boolean; stateRestored: boolean; contextRestored: boolean; source: string; error: string; cases: OriginalLinePathGpuCase[]; }
export interface CheckResult { name: string; passed: boolean; detail: string; }
export interface UtilityReport { passed: boolean; checks: CheckResult[]; error: string; }
export interface GraphicsStatus {
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
