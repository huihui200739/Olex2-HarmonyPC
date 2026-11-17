// Platform document transport only. It never executes code or scientific workflows.
export const MAXIMUM_ORIGINAL_DOCUMENT_BYTES: number = 8 * 1024 * 1024;
export const ORIGINAL_SELECTION_SOURCE: string = 'original TGlRenderer::SelectObject and original TGlGroup';
export const ORIGINAL_MAIN_GUI_CELL_SOURCE: string = 'original TMainForm::OnCellVisible/TGXApp::SetCellVisible';
export const ORIGINAL_MAIN_GUI_BASIS_SOURCE: string = 'original TMainForm::OnBasisVisible/TGXApp::SetBasisVisible';
export const ORIGINAL_MAIN_GUI_FRAME_SOURCE: string = 'original TMainForm::FrameVChange/T3DFrameCtrl::SetVisible';
export const ORIGINAL_MAIN_GUI_SHOW_ALL_SOURCE: string = 'original TMainForm::OnShowAll/GXLibMacros::macFmol';
export const ORIGINAL_MAIN_GUI_CENTER_SOURCE: string = 'original TMainForm::OnModelCenter/GXLibMacros::macCenter';
export const ORIGINAL_MAIN_GUI_VIEW_SOURCE: string = 'original TMainForm::OnViewAlong/GXLibMacros::macMatr';
export const ORIGINAL_MAIN_GUI_VIEW_SELECTION_SOURCE: string = 'original TMainForm::OnBond/OnPlane/GXLibMacros::macSetView';
export const ORIGINAL_MAIN_GUI_LABELS_SOURCE: string = 'original TGXApp::AreLabelsVisible/TGXApp::SetLabelsVisible/stateLabelsVisible';
export const ORIGINAL_MAIN_GUI_STRUCTURE_SOURCE: string = 'original TMainForm::macShowStr/TGXApp::SetStructureVisible';
export const ORIGINAL_MAIN_GUI_GRID_SOURCE: string = 'original TMainForm::GridVChange/TGXApp::ShowGrid';
export const ORIGINAL_MAIN_GUI_HIDE_SOURCE: string = 'original TMainForm::OnGraphics/GXLibMacros::macHide';
export const ORIGINAL_MAIN_GUI_QUALITY_SOURCE: string = 'original TMainForm::OnDrawQChange/GXLibMacros::macQual/TGXApp::Quality';
export const ORIGINAL_MAIN_GUI_HYDROGEN_SOURCE: string = 'original GXLibMacros::macShowH/TGXApp hydrogen visibility setters';
export const ORIGINAL_MAIN_GUI_QPEAK_SOURCE: string = 'original GXLibMacros::macShowQ/TGXApp Q-peak visibility setters';
export const ORIGINAL_MAIN_GUI_FRAGMENT_SOURCE: string = 'original TMainForm fragment actions/TGXApp::FragmentsVisible and SelectFragments';
export interface OriginalDocumentCoreStatus {
  passed: boolean; loaded: boolean; name: string; format: string;
  byteCount: number; sha256: string; revision: number; frames: number;
  atomCount: number; bondCount: number; exportByteCount: number;
  renderObjects?: number; renderPrimitives?: number; renderCollections?: number; bondRadius?: number;
  error: string; modelMayHaveChanged: boolean; source: string;
  // Optional for pre-operation and failure envelopes, never synthesized in UI.
  selectedObjectCount?: number; selectedParts?: string; selectionSequence?: number; pickHit?: boolean; viewHash?: string;
  lastPickedCollection?: string; selectionAction?: string; selectionSource?: string;
  labelsVisible?: boolean; hydrogensVisible?: boolean; hydrogenBondsVisible?: boolean;
  qPeaksVisible?: boolean; qPeakBondsVisible?: boolean; structureVisible?: boolean;
  cellVisible?: boolean; basisVisible?: boolean; frameVisible?: boolean; gridVisible?: boolean;
}
export interface OriginalDocumentEnvelope {
  passed: boolean; surfacePresented: boolean; parentContextRestored: boolean;
  error: string; core: OriginalDocumentCoreStatus; sessionRetained?: boolean;
}
export interface OriginalDocumentTransferStatus {
  sequence: number; operation: string; state: string; selectedUri: string;
  name: string; byteCount: number; sourceKind: string; nativeInvoked: boolean;
  destinationMayBePartial: boolean; error: string; nativeStatusError: string;
}
export interface OriginalDocumentTransferResult {
  status: OriginalDocumentTransferStatus; document: OriginalDocumentEnvelope;
}
export interface OriginalDocumentBackend {
  open(name: string, bytes: Uint8Array): string;
  close(): string;
  status(): string;
  snapshot(): Uint8Array;
}
export interface OriginalDocumentPort {
  selectImport(): Promise<string[]>;
  selectExport(name: string): Promise<string[]>;
  read(uri: string): Uint8Array;
  writeVerified(uri: string, bytes: Uint8Array): void;
}
export interface OriginalDocumentByteStream {
  read(maximum: number): Uint8Array;
  write(bytes: Uint8Array): number;
  seekStart(): void;
  flush(): void;
}
export function emptyOriginalDocument(): OriginalDocumentEnvelope {
  return { passed: false, surfacePresented: false, parentContextRestored: false, error: '',
    core: { passed: false, loaded: false, name: '', format: '', byteCount: 0, sha256: '',
      revision: 0, frames: 0, atomCount: 0, bondCount: 0, exportByteCount: 0, error: '',
      modelMayHaveChanged: false, source: '' } };
}
export function emptyOriginalDocumentTransfer(): OriginalDocumentTransferStatus {
  return { sequence: 0, operation: '', state: 'idle', selectedUri: '', name: '', byteCount: 0,
    sourceKind: '', nativeInvoked: false, destinationMayBePartial: false, error: '', nativeStatusError: '' };
}
function validCounter(value: number): boolean {
  return Number.isSafeInteger(value) && value >= 0;
}
// Native collection names are at most 128 complete Unicode scalar values
// (at most 512 UTF-8 bytes). Reject unpaired UTF-16 surrogates after JSON decode.
function validPickedCollection(value: string): boolean {
  let characters: number = 0;
  let bytes: number = 0;
  for (let i: number = 0; i < value.length; i++) {
    const code: number = value.charCodeAt(i);
    characters++;
    if (code < 0x80) bytes++;
    else if (code < 0x800) bytes += 2;
    else if (code >= 0xD800 && code <= 0xDBFF) {
      const low: number = value.charCodeAt(++i);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) return false;
      bytes += 4;
    } else if (code >= 0xDC00 && code <= 0xDFFF) return false;
    else bytes += 3;
    if (characters > 128 || bytes > 512) return false;
  }
  return true;
}
function validateOptionalSelection(core: OriginalDocumentCoreStatus): void {
  const fields: string[] = Object.keys(core);
  if ((fields.indexOf('selectedObjectCount') >= 0 && !validCounter(core.selectedObjectCount as number)) ||
    (fields.indexOf('selectionSequence') >= 0 && !validCounter(core.selectionSequence as number)) ||
    (fields.indexOf('pickHit') >= 0 && typeof core.pickHit !== 'boolean') ||
    (fields.indexOf('labelsVisible') >= 0 && typeof core.labelsVisible !== 'boolean') ||
    (fields.indexOf('hydrogensVisible') >= 0 && typeof core.hydrogensVisible !== 'boolean') ||
    (fields.indexOf('hydrogenBondsVisible') >= 0 && typeof core.hydrogenBondsVisible !== 'boolean') ||
    (fields.indexOf('qPeaksVisible') >= 0 && typeof core.qPeaksVisible !== 'boolean') ||
    (fields.indexOf('qPeakBondsVisible') >= 0 && typeof core.qPeakBondsVisible !== 'boolean') ||
    (fields.indexOf('structureVisible') >= 0 && typeof core.structureVisible !== 'boolean') ||
    (fields.indexOf('cellVisible') >= 0 && typeof core.cellVisible !== 'boolean') ||
    (fields.indexOf('basisVisible') >= 0 && typeof core.basisVisible !== 'boolean') ||
    (fields.indexOf('frameVisible') >= 0 && typeof core.frameVisible !== 'boolean') ||
    (fields.indexOf('gridVisible') >= 0 && typeof core.gridVisible !== 'boolean') ||
    (fields.indexOf('lastPickedCollection') >= 0 && (typeof core.lastPickedCollection !== 'string' ||
      !validPickedCollection(core.lastPickedCollection))) ||
    (fields.indexOf('selectionAction') >= 0 && (typeof core.selectionAction !== 'string' ||
      (core.selectionAction !== '' && core.selectionAction !== 'pick' && core.selectionAction !== 'clear-selection' &&
       core.selectionAction !== 'select-all' && core.selectionAction !== 'invert-selection' && core.selectionAction !== 'restore-selection' &&
       core.selectionAction !== 'toggle-cell' && core.selectionAction !== 'toggle-basis' && core.selectionAction !== 'toggle-frame' &&
       core.selectionAction !== 'show-all' && core.selectionAction !== 'center-model' && core.selectionAction !== 'view-along-100' &&
       core.selectionAction !== 'view-along-010' && core.selectionAction !== 'view-along-001' &&
       core.selectionAction !== 'view-along-110' && core.selectionAction !== 'view-along-101' &&
       core.selectionAction !== 'view-along-011' && core.selectionAction !== 'view-along-111' &&
       core.selectionAction !== 'view-along-selection' && core.selectionAction !== 'hide-selection' &&
       core.selectionAction !== 'toggle-grid' && core.selectionAction !== 'quality-1' &&
       core.selectionAction !== 'quality-2' && core.selectionAction !== 'quality-3' &&
       core.selectionAction !== 'toggle-labels' && core.selectionAction !== 'toggle-hydrogens' &&
       core.selectionAction !== 'toggle-hydrogen-bonds' && core.selectionAction !== 'toggle-qpeaks' &&
       core.selectionAction !== 'toggle-qpeak-bonds' && core.selectionAction !== 'toggle-structure' &&
       core.selectionAction !== 'fragment-select' && core.selectionAction !== 'fragment-select-atoms' &&
       core.selectionAction !== 'fragment-select-bonds' && core.selectionAction !== 'fragment-hide' &&
       core.selectionAction !== 'fragment-show-only'))) ||
    (fields.indexOf('selectionSource') >= 0 && (typeof core.selectionSource !== 'string' ||
      core.selectionSource !== ORIGINAL_SELECTION_SOURCE && core.selectionSource !== ORIGINAL_MAIN_GUI_CELL_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_BASIS_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_FRAME_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_SHOW_ALL_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_CENTER_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_VIEW_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_VIEW_SELECTION_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_LABELS_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_GRID_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_HIDE_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_QUALITY_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_HYDROGEN_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_QPEAK_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_STRUCTURE_SOURCE &&
      core.selectionSource !== ORIGINAL_MAIN_GUI_FRAGMENT_SOURCE))) {
    throw new Error('原版文档选择状态格式不合法');
  }
}
export function parseOriginalDocument(raw: string): OriginalDocumentEnvelope {
  if (raw.length === 0 || raw.length > 64 * 1024) throw new Error('原版文档状态为空或过长');
  const result: OriginalDocumentEnvelope = JSON.parse(raw) as OriginalDocumentEnvelope;
  if (result === null || typeof result !== 'object' || typeof result.passed !== 'boolean' ||
    typeof result.surfacePresented !== 'boolean' || typeof result.parentContextRestored !== 'boolean' ||
    typeof result.error !== 'string' || result.core === null || typeof result.core !== 'object') {
    throw new Error('原版文档状态格式不完整');
  }
  if (Object.keys(result).indexOf('sessionRetained') >= 0 && typeof result.sessionRetained !== 'boolean') {
    throw new Error('原版文档会话保留状态格式不合法');
  }
  const core: OriginalDocumentCoreStatus = result.core;
  if (typeof core.passed !== 'boolean' || typeof core.loaded !== 'boolean' || typeof core.name !== 'string' ||
    typeof core.format !== 'string' || typeof core.sha256 !== 'string' || typeof core.error !== 'string' ||
    typeof core.modelMayHaveChanged !== 'boolean' || typeof core.source !== 'string' ||
    !validCounter(core.byteCount) || !validCounter(core.revision) || !validCounter(core.frames) ||
    !validCounter(core.atomCount) || !validCounter(core.bondCount) || !validCounter(core.exportByteCount) ||
    (core.renderObjects !== undefined && !validCounter(core.renderObjects)) ||
    (core.renderPrimitives !== undefined && !validCounter(core.renderPrimitives)) ||
    (core.renderCollections !== undefined && !validCounter(core.renderCollections)) ||
    (core.bondRadius !== undefined && (typeof core.bondRadius !== 'number' || !Number.isFinite(core.bondRadius)))) {
    throw new Error('原版文档核心状态格式不完整');
  }
  if (core.viewHash !== undefined && (typeof core.viewHash !== 'string' || core.viewHash.length > 32)) {
    throw new Error('原版文档视角状态格式不合法');
  }
  if (core.selectedParts !== undefined && typeof core.selectedParts !== 'string') {
    throw new Error('原版文档选择 Part 状态格式不合法');
  }
  validateOptionalSelection(core);
  return result;
}
export function originalDocumentPresented(document: OriginalDocumentEnvelope): boolean {
  try { requireNativeSuccess(document, true); return true; }
  catch (_) { return false; }
}
function requireNativeSuccess(document: OriginalDocumentEnvelope, loaded: boolean): void {
  if (!document.passed || !document.surfacePresented || !document.parentContextRestored ||
    !document.core.passed || document.core.loaded !== loaded || document.error.length > 0 ||
    document.core.error.length > 0 || document.core.modelMayHaveChanged) {
    throw new Error(document.error || document.core.error || '原版文档操作未完成；请查看原生状态');
  }
  if (loaded) {
    validateOriginalDocumentName(document.core.name);
    if (document.core.format !== originalDocumentFormat(document.core.name) || document.core.byteCount === 0 ||
      document.core.byteCount > MAXIMUM_ORIGINAL_DOCUMENT_BYTES || !/^[0-9a-f]{64}$/i.test(document.core.sha256) ||
      document.core.frames === 0 || document.core.source !== 'original TGXApp/TGXFile + original TGlRenderer') {
      throw new Error('原版文档成功状态缺少实际载入 / 显示来源信息');
    }
  }
}
export function validateOriginalDocumentBytes(bytes: Uint8Array): void {
  if (bytes.byteLength === 0 || bytes.byteLength > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) {
    throw new Error('原版文档必须为 1 字节至 8 MiB');
  }
  let at: number = 0;
  while (at < bytes.byteLength) {
    const first: number = bytes[at++];
    let point: number = first;
    let remaining: number = 0;
    let minimum: number = 0;
    if (first >= 0xC2 && first <= 0xDF) { point = first & 31; remaining = 1; minimum = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { point = first & 15; remaining = 2; minimum = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { point = first & 7; remaining = 3; minimum = 0x10000; }
    else if (first >= 0x80) throw new Error('原版文档仅支持有效 UTF-8 文本');
    if (at + remaining > bytes.byteLength) throw new Error('原版文档 UTF-8 字符不完整');
    for (let i: number = 0; i < remaining; i++) {
      const next: number = bytes[at++];
      if (next < 0x80 || next > 0xBF) throw new Error('原版文档仅支持有效 UTF-8 文本');
      point = point * 64 + (next & 63);
    }
    if (point === 0) throw new Error('原版文档不能包含 NUL 字符');
    if (point < minimum || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) {
      throw new Error('原版文档仅支持有效 UTF-8 文本');
    }
  }
}
export function originalDocumentFormat(name: string): string {
  const dot: number = name.lastIndexOf('.');
  return dot < 0 ? '' : name.substring(dot + 1).toLowerCase();
}
export function validateOriginalDocumentName(name: string): void {
  if (name.length === 0 || name.length > 240 || /[\x00-\x1F\x7F/\\]/.test(name)) {
    throw new Error('文档名称必须为不含路径分隔符的文件名');
  }
  let length: number = 0;
  for (let i: number = 0; i < name.length; i++) {
    const code: number = name.charCodeAt(i);
    if (code < 0x80) length++;
    else if (code < 0x800) length += 2;
    else if (code >= 0xD800 && code <= 0xDBFF) {
      const low: number = name.charCodeAt(++i);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) throw new Error('文档名称包含不完整 Unicode 字符');
      length += 4;
    } else if (code >= 0xDC00 && code <= 0xDFFF) throw new Error('文档名称包含不完整 Unicode 字符');
    else length += 3;
    if (length > 240) throw new Error('文档名称不能超过 240 个 UTF-8 字节');
  }
  const format: string = originalDocumentFormat(name);
  if (format !== 'cif' && format !== 'ins' && format !== 'res' && format !== 'xyz' && format !== 'pdb' && format !== 'mol') throw new Error('仅支持 .cif / .ins / .res / .xyz / .pdb / .mol 文档');
}
function requireDocumentUri(uri: string): void {
  if (!uri.startsWith('file://') || uri.length > 16 * 1024 || /[\x00-\x1F\x7F]/.test(uri)) {
    throw new Error('请选择系统文件选择器提供的文档 URI');
  }
}
export function originalDocumentNameFromUri(uri: string): string {
  requireDocumentUri(uri);
  // Only decode the URI's final component for a DISPLAY basename. Do not map a
  // provider URI to a shell path, app-private path or assumed physical location.
  const withoutQuery: string = uri.split('?')[0].split('#')[0];
  const name: string = decodeURIComponent(withoutQuery.substring(withoutQuery.lastIndexOf('/') + 1));
  validateOriginalDocumentName(name);
  return name;
}
export function readOriginalDocumentStream(stream: OriginalDocumentByteStream): Uint8Array {
  const chunks: Uint8Array[] = [];
  let length: number = 0;
  while (true) {
    const maximum: number = Math.min(64 * 1024, MAXIMUM_ORIGINAL_DOCUMENT_BYTES + 1 - length);
    const part: Uint8Array = stream.read(maximum);
    if (part.byteLength > maximum || length + part.byteLength > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) {
      throw new Error('原版文档不能超过 8 MiB');
    }
    if (part.byteLength === 0) break;
    chunks.push(part.slice()); length += part.byteLength;
  }
  const bytes: Uint8Array = new Uint8Array(length);
  let offset: number = 0;
  for (const part of chunks) { bytes.set(part, offset); offset += part.byteLength; }
  return bytes;
}
export function writeOriginalDocumentStream(stream: OriginalDocumentByteStream, bytes: Uint8Array): void {
  validateOriginalDocumentBytes(bytes);
  let written: number = 0;
  while (written < bytes.byteLength) {
    const count: number = stream.write(bytes.slice(written));
    if (!Number.isInteger(count) || count <= 0 || count > bytes.byteLength - written) throw new Error('原版文档写入未完成');
    written += count;
  }
  stream.flush(); stream.seekStart();
  const actual: Uint8Array = readOriginalDocumentStream(stream);
  if (actual.byteLength !== bytes.byteLength) throw new Error('保存后的文档长度不一致');
  for (let i: number = 0; i < actual.byteLength; i++) {
    if (actual[i] !== bytes[i]) throw new Error('保存后的文档内容不一致');
  }
}
export class OriginalDocumentTransfer {
  private active: boolean = false;
  private sequence: number = 0;
  private current: OriginalDocumentTransferResult = { status: emptyOriginalDocumentTransfer(), document: emptyOriginalDocument() };

  async run(operation: string, backend: OriginalDocumentBackend, port: OriginalDocumentPort): Promise<OriginalDocumentTransferResult> {
    if (this.active) return this.current;
    const result: OriginalDocumentTransferResult = this.begin(operation, 'system-document');
    let writeStarted: boolean = false;
    let nativeResponseReceived: boolean = false;
    try {
      result.document = parseOriginalDocument(backend.status());
      if (operation === 'close') {
        result.status.state = 'native'; result.status.nativeInvoked = true;
        result.document = parseOriginalDocument(backend.close());
        nativeResponseReceived = true;
        requireNativeSuccess(result.document, false);
      } else if (operation === 'import' || operation === 'export') {
        let snapshot: Uint8Array = new Uint8Array(0);
        if (operation === 'export') {
          if (!result.document.core.loaded) throw new Error('请先打开原版文档');
          result.status.name = result.document.core.name;
          validateOriginalDocumentName(result.status.name);
          result.status.nativeInvoked = true;
          // The original native saver must finish BEFORE the save picker opens.
          snapshot = backend.snapshot().slice();
          validateOriginalDocumentBytes(snapshot);
          result.document = parseOriginalDocument(backend.status());
          nativeResponseReceived = true;
          requireNativeSuccess(result.document, true);
          if (result.document.core.exportByteCount !== snapshot.byteLength) throw new Error('原版保存器返回的字节数与状态不一致');
          result.status.byteCount = snapshot.byteLength;
        }
        result.status.state = 'choosing';
        const uris: string[] = operation === 'import' ? await port.selectImport() : await port.selectExport(result.status.name);
        if (uris.length === 0) { result.status.state = 'cancelled'; return result; }
        if (uris.length !== 1) throw new Error('请选择一个系统文档');
        requireDocumentUri(uris[0]); result.status.selectedUri = uris[0];
        if (operation === 'import') {
          result.status.name = originalDocumentNameFromUri(uris[0]);
          const bytes: Uint8Array = port.read(uris[0]);
          validateOriginalDocumentBytes(bytes); result.status.byteCount = bytes.byteLength;
          result.status.state = 'native'; result.status.nativeInvoked = true;
          result.document = parseOriginalDocument(backend.open(result.status.name, bytes));
          nativeResponseReceived = true;
          requireNativeSuccess(result.document, true);
          if (result.document.core.name !== result.status.name || result.document.core.byteCount !== bytes.byteLength) {
            throw new Error('原版文档载入状态与所选文件不一致');
          }
        } else {
          // Do not silently rename a saver output to another file format.
          const savedName: string = originalDocumentNameFromUri(uris[0]);
          if (originalDocumentFormat(savedName) !== originalDocumentFormat(result.status.name)) {
            throw new Error('另存文件的扩展名必须与原版保存格式一致');
          }
          result.status.state = 'writing'; writeStarted = true;
          port.writeVerified(uris[0], snapshot);
        }
      } else throw new Error('不支持的原版文档操作');
      result.status.state = 'passed';
    } catch (error) {
      result.status.state = 'failed'; result.status.error = (error as Error).message || '原版文档操作未完成';
      result.status.destinationMayBePartial = writeStarted;
      if (!nativeResponseReceived) this.refreshAfterFailure(result, backend);
    } finally { this.active = false; }
    return result;
  }

  async openSynthetic(bytes: Uint8Array, backend: OriginalDocumentBackend,
    name: string = 'synthetic-display.cif'): Promise<OriginalDocumentTransferResult> {
    return this.openBytes(bytes, backend, name, 'synthetic-platform-display', '合成显示示例');
  }

  async reopenRecorded(bytes: Uint8Array, backend: OriginalDocumentBackend, name: string): Promise<OriginalDocumentTransferResult> {
    return this.openBytes(bytes, backend, name, 'recent-original-source', '最近文件');
  }

  private async openBytes(bytes: Uint8Array, backend: OriginalDocumentBackend, name: string,
    sourceKind: string, label: string): Promise<OriginalDocumentTransferResult> {
    if (this.active) return this.current;
    const result: OriginalDocumentTransferResult = this.begin('import', sourceKind);
    result.status.name = name;
    let nativeResponseReceived: boolean = false;
    try {
      result.document = parseOriginalDocument(backend.status());
      validateOriginalDocumentBytes(bytes); result.status.byteCount = bytes.byteLength;
      result.status.state = 'native'; result.status.nativeInvoked = true;
      result.document = parseOriginalDocument(backend.open(result.status.name, bytes));
      nativeResponseReceived = true;
      requireNativeSuccess(result.document, true);
      if (result.document.core.name !== result.status.name || result.document.core.byteCount !== bytes.byteLength) {
        throw new Error(label + '的原版载入状态不一致');
      }
      result.status.state = 'passed';
    } catch (error) {
      result.status.state = 'failed'; result.status.error = (error as Error).message || (label + '未能打开');
      if (!nativeResponseReceived) this.refreshAfterFailure(result, backend);
    } finally { this.active = false; }
    return result;
  }

  private begin(operation: string, sourceKind: string): OriginalDocumentTransferResult {
    this.active = true;
    const status: OriginalDocumentTransferStatus = emptyOriginalDocumentTransfer();
    status.sequence = ++this.sequence; status.operation = operation; status.sourceKind = sourceKind; status.state = 'preparing';
    const result: OriginalDocumentTransferResult = { status: status, document: this.current.document };
    this.current = result;
    return result;
  }
  private refreshAfterFailure(result: OriginalDocumentTransferResult, backend: OriginalDocumentBackend): void {
    if (!result.status.nativeInvoked) return;
    try { result.document = parseOriginalDocument(backend.status()); }
    catch (error) { result.status.nativeStatusError = (error as Error).message || '无法读取原版文档缓存状态'; }
  }
}
