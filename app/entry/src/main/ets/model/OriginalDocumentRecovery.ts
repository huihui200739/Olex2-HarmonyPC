// Ordinary text/session metadata only. No file/native I/O, clocks, revisions,
// presentation envelopes, application commands, macros or application Python.
import { encodeOriginalDocumentText } from './OriginalDocumentEditor';
import { OriginalDocumentViewportState, copyOriginalDocumentViewport, originalDocumentViewportMatchesSource } from './OriginalDocumentViewport';
import { MAXIMUM_ORIGINAL_DOCUMENT_BYTES, originalDocumentFormat, validateOriginalDocumentName } from './OriginalDocumentTransfer';
import { OriginalDocumentSaveTarget, OriginalDocumentSaveTargets } from './OriginalDocumentSaveTargets';
import { OriginalTextHistoryRecovery, OriginalTextPatch, validateOriginalTextHistoryRecovery } from './OriginalTextTools';

export interface OriginalDocumentRecoveryUi {
  editorVisible: boolean;
  findVisible: boolean;
  query: string;
  replacement: string;
  selectionStart: number;
  selectionEnd: number;
  toolsAction: string;
  toolsSequence: number;
  replaceCount: number;
}

export interface OriginalDocumentRecoveryDocument {
  id: string;
  name: string;
  sourceKind: string;
  sourceText: string;
  sourceByteCount: number;
  sourceSha256: string;
  editorInitialized: boolean;
  draft: string;
  documentDirty: boolean;
  history: OriginalTextHistoryRecovery;
  ui: OriginalDocumentRecoveryUi;
  target?: OriginalDocumentSaveTarget;
  viewport?: OriginalDocumentViewportState;
}

export interface OriginalDocumentRecoverySnapshot {
  // Legacy 1 is accepted as input only; validation/decoding return canonical schema 2.
  schemaVersion: 1 | 2;
  generation: number;
  savedAt: string;
  activeId: string;
  documents: OriginalDocumentRecoveryDocument[];
}

interface OriginalDocumentRecoveryEnvelope {
  schemaVersion: 1 | 2;
  payload: string;
  payloadSha256: string;
}

export const MAXIMUM_ORIGINAL_RECOVERY_BYTES: number = 256 * 1024 * 1024;

function requireShape(value: object, required: string[], optional: string[] = []): void {
  if (value === null || typeof value !== 'object' || Array.isArray(value)) throw new Error('恢复数据必须为对象');
  const fields: string[] = Object.keys(value);
  for (let i: number = 0; i < required.length; i++) {
    if (fields.indexOf(required[i]) < 0) throw new Error('恢复数据缺少必需字段');
  }
  for (let i: number = 0; i < fields.length; i++) {
    if (required.indexOf(fields[i]) < 0 && optional.indexOf(fields[i]) < 0) {
      throw new Error('恢复数据含未知字段；不接受旧 native 状态或 revision');
    }
  }
}

function requireCounter(value: number): void {
  if (!Number.isSafeInteger(value) || value < 0) throw new Error('恢复计数必须为非负安全整数');
}

function requireId(id: string): void {
  if (typeof id !== 'string' || id.length > 25 || !/^document-[1-9][0-9]*$/.test(id)) {
    throw new Error('恢复 ID 必须为规范 document-N');
  }
  const number: number = Number(id.substring(9));
  if (!Number.isSafeInteger(number) || !Number.isSafeInteger(number + 1)) throw new Error('恢复 ID 无法安全递增');
}

function requireSha(sha: string): void {
  if (typeof sha !== 'string' || !/^[0-9a-f]{64}$/.test(sha)) throw new Error('恢复摘要必须为 64 位小写 SHA256');
}

function requireSavedAt(savedAt: string): void {
  // Parse only caller-supplied UTC calendar time; never obtain the current time.
  if (typeof savedAt !== 'string' || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{3})?Z$/.test(savedAt)) {
    throw new Error('恢复 savedAt 必须为显式 ISO UTC 时间');
  }
  const value: number = Date.parse(savedAt);
  const canonical: string = savedAt.length === 20 ? savedAt.slice(0, 19) + '.000Z' : savedAt;
  if (!Number.isFinite(value) || new Date(value).toISOString() !== canonical) throw new Error('恢复 savedAt 日历时间不合法');
}

// A generic strict UTF-8 encoder for JSON, NOT the 8 MiB/NUL-free document
// encoder. JSON.stringify escapes transient lone surrogates before this call.
function recoveryUtf8Length(text: string): number {
  if (typeof text !== 'string' || text.length > MAXIMUM_ORIGINAL_RECOVERY_BYTES) {
    throw new Error('恢复文本超出 256 MiB 上限或不是文本');
  }
  let length: number = 0;
  for (let i: number = 0; i < text.length; i++) {
    const code: number = text.charCodeAt(i);
    if (code < 0x80) length++;
    else if (code < 0x800) length += 2;
    else if (code >= 0xD800 && code <= 0xDBFF) {
      const low: number = text.charCodeAt(++i);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) throw new Error('恢复文本不是完整 Unicode');
      length += 4;
    } else if (code >= 0xDC00 && code <= 0xDFFF) throw new Error('恢复文本不是完整 Unicode');
    else length += 3;
    if (length > MAXIMUM_ORIGINAL_RECOVERY_BYTES) throw new Error('恢复 UTF-8 超出 256 MiB 上限');
  }
  return length;
}

export function recoveryUtf8Bytes(text: string): Uint8Array {
  const bytes: Uint8Array = new Uint8Array(recoveryUtf8Length(text));
  let at: number = 0;
  for (let i: number = 0; i < text.length; i++) {
    let point: number = text.charCodeAt(i);
    if (point >= 0xD800 && point <= 0xDBFF) {
      point = 0x10000 + (point - 0xD800) * 1024 + text.charCodeAt(++i) - 0xDC00;
    }
    if (point < 0x80) bytes[at++] = point;
    else if (point < 0x800) {
      bytes[at++] = 0xC0 + (point >> 6); bytes[at++] = 0x80 + (point & 63);
    } else if (point < 0x10000) {
      bytes[at++] = 0xE0 + (point >> 12); bytes[at++] = 0x80 + ((point >> 6) & 63); bytes[at++] = 0x80 + (point & 63);
    } else {
      bytes[at++] = 0xF0 + (point >> 18); bytes[at++] = 0x80 + ((point >> 12) & 63);
      bytes[at++] = 0x80 + ((point >> 6) & 63); bytes[at++] = 0x80 + (point & 63);
    }
  }
  return bytes;
}

// Exact canonical JSON size accounting BEFORE a large stringify/allocation.
// outer counts the bytes of payload as an escaped JSON string, without its
// enclosing quotes. This includes both escaping levels, including lone IME
// surrogates. The fixed envelope is accounted for by assertFits().
class RecoveryJsonBudget {
  private payload: number = 0;
  private outer: number = 0;

  syntax(text: string): void {
    this.payload += text.length;
    this.outer += text.length;
    for (let i: number = 0; i < text.length; i++) {
      if (text.charAt(i) === '"' || text.charAt(i) === '\\') this.outer++;
    }
    this.assertFits();
  }

  text(text: string): void {
    this.payload += 2;
    this.outer += 4;
    for (let i: number = 0; i < text.length; i++) {
      const code: number = text.charCodeAt(i);
      if (code === 34 || code === 92) { this.payload += 2; this.outer += 4; }
      else if (code === 8 || code === 9 || code === 10 || code === 12 || code === 13) {
        this.payload += 2; this.outer += 3;
      } else if (code < 32) { this.payload += 6; this.outer += 7; }
      else if (code >= 0xD800 && code <= 0xDFFF) {
        const low: number = text.charCodeAt(i + 1);
        if (code <= 0xDBFF && low >= 0xDC00 && low <= 0xDFFF) {
          this.payload += 4; this.outer += 4; i++;
        } else { this.payload += 6; this.outer += 7; }
      } else {
        const size: number = code < 0x80 ? 1 : code < 0x800 ? 2 : 3;
        this.payload += size; this.outer += size;
      }
    }
    this.assertFits();
  }

  field(name: string, value: string): void {
    this.syntax(',"' + name + '":'); this.text(value);
  }

  assertFits(): void {
    const envelopeBytes: number = '{"schemaVersion":2,"payload":"","payloadSha256":""}'.length + 64;
    if (this.payload > MAXIMUM_ORIGINAL_RECOVERY_BYTES || this.outer + envelopeBytes > MAXIMUM_ORIGINAL_RECOVERY_BYTES) {
      throw new Error('恢复序列化整体超出 256 MiB 上限');
    }
  }
}

function budgetHistory(budget: RecoveryJsonBudget, history: OriginalTextHistoryRecovery): void {
  budget.syntax('{"current":'); budget.text(history.current);
  const groups: string[] = ['undoPatches', 'redoPatches'];
  const patches: OriginalTextHistoryRecovery = history;
  for (let group: number = 0; group < groups.length; group++) {
    budget.syntax(',"' + groups[group] + '":[');
    const list: OriginalTextPatch[] = group === 0 ? patches.undoPatches : patches.redoPatches;
    for (let i: number = 0; i < list.length; i++) {
      if (i > 0) budget.syntax(',');
      budget.syntax('{"start":' + list[i].start);
      budget.field('removed', list[i].removed); budget.field('inserted', list[i].inserted); budget.syntax('}');
    }
    budget.syntax(']');
  }
  budget.syntax(',"maxSteps":' + history.maxSteps + ',"maxCharacters":' + history.maxCharacters + '}');
}

function budgetViewport(budget: RecoveryJsonBudget, viewport: OriginalDocumentViewportState): void {
  budget.syntax('{"schemaVersion":1'); budget.field('name', viewport.name);
  budget.syntax(',"byteCount":' + viewport.byteCount); budget.field('sha256', viewport.sha256);
  budget.syntax(',"yaw":' + viewport.yaw + ',"pitch":' + viewport.pitch + ',"distance":' + viewport.distance);
  budget.syntax(',"objectCount":' + viewport.objectCount); budget.field('inventorySha256', viewport.inventorySha256);
  budget.syntax(',"selectedObjectIndices":[');
  for (let i: number = 0; i < viewport.selectedObjectIndices.length; i++) {
    if (i > 0) budget.syntax(',');
    budget.syntax('' + viewport.selectedObjectIndices[i]);
  }
  budget.syntax(']}');
}

function budgetSnapshot(snapshot: OriginalDocumentRecoverySnapshot): void {
  const budget: RecoveryJsonBudget = new RecoveryJsonBudget();
  budget.syntax('{"schemaVersion":2,"generation":' + snapshot.generation);
  budget.field('savedAt', snapshot.savedAt); budget.field('activeId', snapshot.activeId); budget.syntax(',"documents":[');
  for (let i: number = 0; i < snapshot.documents.length; i++) {
    const doc: OriginalDocumentRecoveryDocument = snapshot.documents[i];
    if (i > 0) budget.syntax(',');
    budget.syntax('{"id":'); budget.text(doc.id); budget.field('name', doc.name);
    budget.field('sourceKind', doc.sourceKind); budget.field('sourceText', doc.sourceText);
    budget.syntax(',"sourceByteCount":' + doc.sourceByteCount); budget.field('sourceSha256', doc.sourceSha256);
    budget.syntax(',"editorInitialized":' + doc.editorInitialized); budget.field('draft', doc.draft);
    budget.syntax(',"documentDirty":' + doc.documentDirty + ',"history":'); budgetHistory(budget, doc.history);
    budget.syntax(',"ui":{"editorVisible":' + doc.ui.editorVisible + ',"findVisible":' + doc.ui.findVisible);
    budget.field('query', doc.ui.query); budget.field('replacement', doc.ui.replacement);
    budget.syntax(',"selectionStart":' + doc.ui.selectionStart + ',"selectionEnd":' + doc.ui.selectionEnd);
    budget.field('toolsAction', doc.ui.toolsAction);
    budget.syntax(',"toolsSequence":' + doc.ui.toolsSequence + ',"replaceCount":' + doc.ui.replaceCount + '}');
    if (doc.target !== undefined) {
      budget.syntax(',"target":{"id":'); budget.text(doc.target.id); budget.field('uri', doc.target.uri);
      budget.field('name', doc.target.name); budget.syntax(',"byteCount":' + doc.target.byteCount);
      budget.field('sha256', doc.target.sha256); budget.syntax(',"sequence":' + doc.target.sequence + '}');
    }
    if (doc.viewport !== undefined) {
      budget.syntax(',"viewport":'); budgetViewport(budget, doc.viewport);
    }
    budget.syntax('}');
  }
  budget.syntax(']}');
}

function checkedUi(ui: OriginalDocumentRecoveryUi, draft: string): OriginalDocumentRecoveryUi {
  requireShape(ui, ['editorVisible', 'findVisible', 'query', 'replacement', 'selectionStart', 'selectionEnd',
    'toolsAction', 'toolsSequence', 'replaceCount']);
  if (typeof ui.editorVisible !== 'boolean' || typeof ui.findVisible !== 'boolean' ||
    typeof ui.query !== 'string' || ui.query.length > 4096 || typeof ui.replacement !== 'string' ||
    ui.replacement.length > 4096 || typeof ui.toolsAction !== 'string' || ui.toolsAction.length > 4096) {
    throw new Error('恢复 UI 文本或布尔状态不合法');
  }
  requireCounter(ui.selectionStart); requireCounter(ui.selectionEnd);
  requireCounter(ui.toolsSequence); requireCounter(ui.replaceCount);
  if (ui.selectionStart > ui.selectionEnd || ui.selectionEnd > draft.length) throw new Error('恢复选择范围超出草稿');
  return { editorVisible: ui.editorVisible, findVisible: ui.findVisible, query: ui.query, replacement: ui.replacement,
    selectionStart: ui.selectionStart, selectionEnd: ui.selectionEnd, toolsAction: ui.toolsAction,
    toolsSequence: ui.toolsSequence, replaceCount: ui.replaceCount };
}

export function validateOriginalDocumentRecovery(snapshot: OriginalDocumentRecoverySnapshot,
  digest: (bytes: Uint8Array) => string): OriginalDocumentRecoverySnapshot {
  if (typeof digest !== 'function') throw new Error('恢复需要显式摘要函数');
  requireShape(snapshot, ['schemaVersion', 'generation', 'savedAt', 'activeId', 'documents']);
  if ((snapshot.schemaVersion !== 1 && snapshot.schemaVersion !== 2) || !Number.isSafeInteger(snapshot.generation) || snapshot.generation < 1 ||
    typeof snapshot.activeId !== 'string' || !Array.isArray(snapshot.documents) || snapshot.documents.length > 4) {
    throw new Error('恢复版本、代数、活动 ID 或文档数量不合法');
  }
  requireSavedAt(snapshot.savedAt);
  const documents: OriginalDocumentRecoveryDocument[] = [];
  const targets: OriginalDocumentSaveTargets = new OriginalDocumentSaveTargets();
  let total: number = 0;
  let activeFound: boolean = false;
  for (let i: number = 0; i < snapshot.documents.length; i++) {
    const doc: OriginalDocumentRecoveryDocument = snapshot.documents[i];
    requireShape(doc, ['id', 'name', 'sourceKind', 'sourceText', 'sourceByteCount', 'sourceSha256',
      'editorInitialized', 'draft', 'documentDirty', 'history', 'ui'],
      snapshot.schemaVersion === 1 ? ['target'] : ['target', 'viewport']);
    requireId(doc.id);
    for (let previous: number = 0; previous < documents.length; previous++) {
      if (documents[previous].id === doc.id) throw new Error('恢复文档 ID 重复');
    }
    if (doc.id === snapshot.activeId) activeFound = true;
    if (typeof doc.name !== 'string') throw new Error('恢复文档名称必须为文本');
    validateOriginalDocumentName(doc.name);
    if (typeof doc.sourceKind !== 'string' || doc.sourceKind.length > 128 || doc.sourceKind.trim().length === 0 ||
      /[\x00-\x1F\x7F]/.test(doc.sourceKind) || typeof doc.editorInitialized !== 'boolean' ||
      typeof doc.documentDirty !== 'boolean' || typeof doc.draft !== 'string' || doc.draft.length > MAXIMUM_ORIGINAL_DOCUMENT_BYTES ||
      !Number.isSafeInteger(doc.sourceByteCount) || doc.sourceByteCount < 1 || doc.sourceByteCount > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) {
      throw new Error('恢复来源、草稿、布尔值或来源容量不合法');
    }
    requireSha(doc.sourceSha256);
    const source: Uint8Array = encodeOriginalDocumentText(doc.sourceText);
    if (source.byteLength !== doc.sourceByteCount || digest(source) !== doc.sourceSha256) {
      throw new Error('恢复来源实际 UTF-8 字节数或 SHA256 不符');
    }
    total += source.byteLength;
    if (total > 32 * 1024 * 1024) throw new Error('恢复来源总容量超出 32 MiB');
    const history: OriginalTextHistoryRecovery = validateOriginalTextHistoryRecovery(doc.history);
    if (doc.editorInitialized) {
      if (history.current !== doc.draft) throw new Error('恢复历史当前文本必须等于编辑草稿');
    } else if (doc.draft !== doc.sourceText || (history.current !== '' && history.current !== doc.draft) ||
      history.undoPatches.length !== 0 || history.redoPatches.length !== 0) {
      throw new Error('未初始化编辑器只允许来源草稿和真实空历史');
    }
    const copy: OriginalDocumentRecoveryDocument = { id: doc.id, name: doc.name, sourceKind: doc.sourceKind,
      sourceText: doc.sourceText, sourceByteCount: doc.sourceByteCount, sourceSha256: doc.sourceSha256,
      editorInitialized: doc.editorInitialized, draft: doc.draft, documentDirty: doc.documentDirty,
      history: history, ui: checkedUi(doc.ui, doc.draft) };
    if (doc.target !== undefined) {
      targets.restoreRemembered(doc.target);
      if (doc.target.id !== doc.id || originalDocumentFormat(doc.target.name) !== originalDocumentFormat(doc.name)) {
        throw new Error('恢复保存目标标签或格式与文档不一致');
      }
      copy.target = targets.get(doc.id);
    }
    // Presence is significant: undefined/null/corrupt viewport must fail, never silently disappear.
    // Legacy schema 1 cannot claim viewport metadata it never stored.
    if (Object.keys(doc).indexOf('viewport') >= 0) {
      const viewport: OriginalDocumentViewportState = copyOriginalDocumentViewport(doc.viewport as OriginalDocumentViewportState);
      if (!originalDocumentViewportMatchesSource(viewport, doc.name, doc.sourceByteCount, doc.sourceSha256)) {
        throw new Error('恢复视口与文档实际来源名称、字节数或摘要不一致');
      }
      copy.viewport = viewport;
    }
    documents.push(copy);
  }
  if ((documents.length === 0 && snapshot.activeId !== '') || (documents.length > 0 && !activeFound)) {
    throw new Error('恢复活动 ID 必须指向实际文档；空快照必须为空 ID');
  }
  const checked: OriginalDocumentRecoverySnapshot = { schemaVersion: 2, generation: snapshot.generation,
    savedAt: snapshot.savedAt, activeId: snapshot.activeId, documents: documents };
  budgetSnapshot(checked);
  return checked;
}

export function encodeOriginalDocumentRecovery(snapshot: OriginalDocumentRecoverySnapshot,
  digest: (bytes: Uint8Array) => string): string {
  const checked: OriginalDocumentRecoverySnapshot = validateOriginalDocumentRecovery(snapshot, digest);
  const payload: string = JSON.stringify(checked);
  const sha: string = digest(recoveryUtf8Bytes(payload));
  requireSha(sha);
  const envelope: OriginalDocumentRecoveryEnvelope = { schemaVersion: 2, payload: payload, payloadSha256: sha };
  const text: string = JSON.stringify(envelope);
  recoveryUtf8Length(text);
  return text;
}

export function decodeOriginalDocumentRecovery(text: string,
  digest: (bytes: Uint8Array) => string): OriginalDocumentRecoverySnapshot {
  if (typeof digest !== 'function') throw new Error('恢复需要显式摘要函数');
  recoveryUtf8Length(text); // Check BEFORE parsing even an attacker-supplied envelope.
  const envelope: OriginalDocumentRecoveryEnvelope = JSON.parse(text) as OriginalDocumentRecoveryEnvelope;
  requireShape(envelope, ['schemaVersion', 'payload', 'payloadSha256']);
  if ((envelope.schemaVersion !== 1 && envelope.schemaVersion !== 2) || typeof envelope.payload !== 'string') {
    throw new Error('恢复封装版本或 payload 不合法');
  }
  requireSha(envelope.payloadSha256);
  if (digest(recoveryUtf8Bytes(envelope.payload)) !== envelope.payloadSha256) throw new Error('恢复 payload SHA256 不符');
  const snapshot: OriginalDocumentRecoverySnapshot = JSON.parse(envelope.payload) as OriginalDocumentRecoverySnapshot;
  if (snapshot === null || typeof snapshot !== 'object' || snapshot.schemaVersion !== envelope.schemaVersion) {
    throw new Error('恢复封装与 payload 版本不一致');
  }
  // Normalize only the returned in-memory copy. No rewrite/viewport invention for legacy slots.
  return validateOriginalDocumentRecovery(snapshot, digest);
}
