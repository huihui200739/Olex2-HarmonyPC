// Pure text/draft state only: no native calls, file access or document parsing.
import {
  MAXIMUM_ORIGINAL_DOCUMENT_BYTES, OriginalDocumentEnvelope,
  originalDocumentPresented, parseOriginalDocument, validateOriginalDocumentBytes
} from './OriginalDocumentTransfer';

export interface OriginalDocumentEditorStatus {
  initialized: boolean;
  name: string;
  baseRevision: number;
  baseSha256: string;
  baseBytes: number;
  // UTF-16 code units, matching the text editor's string.length (not UTF-8 bytes).
  draftCharacters: number;
  draftDirty: boolean;
  documentDirty: boolean;
  needsSave: boolean;
  sequence: number;
  error: string;
}

export function decodeOriginalDocumentText(bytes: Uint8Array): string {
  if (!(bytes instanceof Uint8Array)) throw new Error('原版文档内容必须为 UTF-8 字节');
  validateOriginalDocumentBytes(bytes);
  // Bounded character chunks avoid both a huge argument spread and a growing
  // string concatenation. Validation above rejects NUL, overlongs and surrogates.
  const chunks: string[] = [];
  const characters: string[] = [];
  let at: number = 0;
  while (at < bytes.byteLength) {
    const first: number = bytes[at++];
    let point: number = first;
    let remaining: number = 0;
    if (first >= 0xF0) { point = first & 7; remaining = 3; }
    else if (first >= 0xE0) { point = first & 15; remaining = 2; }
    else if (first >= 0xC0) { point = first & 31; remaining = 1; }
    for (let i: number = 0; i < remaining; i++) point = point * 64 + (bytes[at++] & 63);
    if (point < 0x10000) characters.push(String.fromCharCode(point));
    else {
      const pair: number = point - 0x10000;
      characters.push(String.fromCharCode(0xD800 + (pair >> 10), 0xDC00 + (pair & 1023)));
    }
    if (characters.length >= 4096) {
      chunks.push(characters.join(''));
      characters.length = 0;
    }
  }
  if (characters.length > 0) chunks.push(characters.join(''));
  return chunks.join('');
}

export function encodeOriginalDocumentText(text: string): Uint8Array {
  if (typeof text !== 'string') throw new Error('原版文档草稿必须为文本');
  if (text.length === 0 || text.length > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) {
    throw new Error('原版文档必须为 1 字节至 8 MiB');
  }
  // Count strictly first, then allocate exactly once. No number[] per document.
  let length: number = 0;
  for (let i: number = 0; i < text.length; i++) {
    const code: number = text.charCodeAt(i);
    if (code === 0) throw new Error('原版文档不能包含 NUL 字符');
    if (code < 0x80) length++;
    else if (code < 0x800) length += 2;
    else if (code >= 0xD800 && code <= 0xDBFF) {
      const low: number = text.charCodeAt(++i);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) {
        throw new Error('原版文档包含不完整 UTF-16 Unicode 字符');
      }
      length += 4;
    } else if (code >= 0xDC00 && code <= 0xDFFF) {
      throw new Error('原版文档包含不完整 UTF-16 Unicode 字符');
    } else length += 3;
    if (length > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) throw new Error('原版文档 UTF-8 内容不能超过 8 MiB');
  }
  const bytes: Uint8Array = new Uint8Array(length);
  let at: number = 0;
  for (let i: number = 0; i < text.length; i++) {
    let point: number = text.charCodeAt(i);
    if (point >= 0xD800 && point <= 0xDBFF) {
      point = 0x10000 + (point - 0xD800) * 1024 + text.charCodeAt(++i) - 0xDC00;
    }
    if (point < 0x80) bytes[at++] = point;
    else if (point < 0x800) {
      bytes[at++] = 0xC0 + (point >> 6);
      bytes[at++] = 0x80 + (point & 63);
    } else if (point < 0x10000) {
      bytes[at++] = 0xE0 + (point >> 12);
      bytes[at++] = 0x80 + ((point >> 6) & 63);
      bytes[at++] = 0x80 + (point & 63);
    } else {
      bytes[at++] = 0xF0 + (point >> 18);
      bytes[at++] = 0x80 + ((point >> 12) & 63);
      bytes[at++] = 0x80 + ((point >> 6) & 63);
      bytes[at++] = 0x80 + (point & 63);
    }
  }
  return bytes;
}

function requirePresentedDocument(document: OriginalDocumentEnvelope): OriginalDocumentEnvelope {
  // Reuse both the existing structural validation and the full native-presented
  // success predicate; a loaded flag alone is not successful presentation.
  const checked: OriginalDocumentEnvelope = parseOriginalDocument(JSON.stringify(document));
  if (!originalDocumentPresented(checked)) throw new Error('原版文档尚未完整成功载入并呈现');
  return checked;
}

function equalDocumentBytes(left: Uint8Array, right: Uint8Array): boolean {
  if (left.byteLength !== right.byteLength) return false;
  for (let i: number = 0; i < left.byteLength; i++) {
    if (left[i] !== right[i]) return false;
  }
  return true;
}

export class OriginalDocumentEditor {
  private initialized: boolean = false;
  private everInitialized: boolean = false;
  private name: string = '';
  private baseRevision: number = 0;
  private baseSha256: string = '';
  private baseBytes: number = 0;
  private baseText: string = '';
  private draft: string = '';
  private documentDirty: boolean = false;
  private sequence: number = 0;
  private error: string = '';
  private preparedText: string = '';
  private preparedBytes: Uint8Array = new Uint8Array(0);

  getText(): string { return this.draft; }

  getStatus(): OriginalDocumentEditorStatus {
    const draftDirty: boolean = this.initialized && this.draft !== this.baseText;
    return {
      initialized: this.initialized, name: this.name, baseRevision: this.baseRevision,
      baseSha256: this.baseSha256, baseBytes: this.baseBytes, draftCharacters: this.draft.length,
      draftDirty: draftDirty, documentDirty: this.documentDirty,
      needsSave: draftDirty || this.documentDirty, sequence: this.sequence, error: this.error
    };
  }

  capture(document: OriginalDocumentEnvelope, bytes: Uint8Array): void {
    this.begin();
    try {
      if (this.documentDirty || this.draft !== this.baseText) {
        throw new Error('仍有未保存的原版文档或草稿；确认替换后请先 reset');
      }
      const checked: OriginalDocumentEnvelope = requirePresentedDocument(document);
      const text: string = decodeOriginalDocumentText(bytes);
      if (bytes.byteLength !== checked.core.byteCount) throw new Error('原生来源字节数与文档状态不一致');
      // Caller must obtain ORIGINAL SOURCE bytes from the native SHA-checked
      // source API, never the original saver/snapshot output. This pure model
      // checks length/UTF-8; it does not claim to verify a native SHA itself.
      this.setBaseline(checked, text);
      this.draft = text;
      this.documentDirty = false;
      this.clearPrepared();
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  restoreRecovered(actualPresentedEnvelope: OriginalDocumentEnvelope, actualSourceBytes: Uint8Array,
    draft: string, documentDirty: boolean): void {
    // Reject before touching even the status of an existing editor. reset does
    // not turn a previously initialized instance into a recovery destination.
    if (this.everInitialized || this.initialized || this.documentDirty || this.draft !== this.baseText) {
      throw new Error('恢复只允许全新且从未初始化的编辑器');
    }
    if (typeof draft !== 'string' || draft.length > MAXIMUM_ORIGINAL_DOCUMENT_BYTES ||
      typeof documentDirty !== 'boolean') throw new Error('恢复草稿容量或脏状态不合法');
    const checked: OriginalDocumentEnvelope = requirePresentedDocument(actualPresentedEnvelope);
    const text: string = decodeOriginalDocumentText(actualSourceBytes);
    if (actualSourceBytes.byteLength !== checked.core.byteCount) throw new Error('恢复来源字节数与本次呈现不一致');
    // No encode of the draft: empty/NUL/partial-surrogate IME text is state,
    // not native input. This model cannot independently attest the native SHA.
    this.setBaseline(checked, text);
    this.draft = draft;
    this.documentDirty = documentDirty;
    this.clearPrepared();
    this.begin();
  }

  reactivate(document: OriginalDocumentEnvelope, bytes: Uint8Array): void {
    this.begin();
    try {
      this.requireInitialized();
      const checked: OriginalDocumentEnvelope = requirePresentedDocument(document);
      if (checked.core.name !== this.name || checked.core.revision <= this.baseRevision) {
        throw new Error('重新激活的文档名称不符或 revision 未递增');
      }
      if (checked.core.sha256 !== this.baseSha256 || checked.core.byteCount !== this.baseBytes) {
        throw new Error('重新激活的文档摘要或字节数与编辑基线不一致');
      }
      const text: string = decodeOriginalDocumentText(bytes);
      if (text !== this.baseText ||
        !equalDocumentBytes(bytes, encodeOriginalDocumentText(this.baseText))) {
        throw new Error('重新激活的来源文字或字节与编辑基线不一致');
      }
      // Validate everything before advancing the native revision. Do not capture
      // again: pending drafts and applied/unsaved state belong to this editor.
      this.baseRevision = checked.core.revision;
      this.clearPrepared();
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  edit(text: string): void {
    this.begin();
    try {
      this.requireInitialized();
      if (typeof text !== 'string') throw new Error('原版文档草稿必须为文本');
      // IME input can temporarily contain an unpaired surrogate, NUL or an
      // empty/oversized draft. Nothing reaches native until strict prepareApply.
      if (text !== this.draft) this.clearPrepared();
      this.draft = text;
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  revertDraft(): void {
    this.begin();
    try {
      this.requireInitialized();
      this.draft = this.baseText;
      this.clearPrepared();
      // An applied but unsaved native document remains dirty.
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  prepareApply(current: OriginalDocumentEnvelope): Uint8Array {
    this.begin();
    this.clearPrepared();
    try {
      this.requireInitialized();
      this.requireCurrent(current);
      if (this.draft === this.baseText) throw new Error('原版文档草稿没有改变');
      const bytes: Uint8Array = encodeOriginalDocumentText(this.draft);
      this.preparedText = this.draft;
      this.preparedBytes = bytes.slice();
      // Returned storage is not our pending transaction's storage.
      return bytes;
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  acceptApplied(current: OriginalDocumentEnvelope, bytes: Uint8Array): void {
    this.begin();
    try {
      this.requireInitialized();
      if (this.preparedBytes.byteLength === 0) throw new Error('请先准备当前草稿的应用字节');
      const checked: OriginalDocumentEnvelope = requirePresentedDocument(current);
      if (checked.core.name !== this.name || checked.core.revision <= this.baseRevision) {
        throw new Error('应用结果文档名称不符或 revision 未递增');
      }
      validateOriginalDocumentBytes(bytes);
      if (checked.core.byteCount !== this.preparedBytes.byteLength ||
        this.draft !== this.preparedText || !equalDocumentBytes(bytes, this.preparedBytes)) {
        throw new Error('应用结果与已准备字节或当前草稿不一致');
      }
      this.setBaseline(checked, this.preparedText);
      this.documentDirty = true;
      this.clearPrepared();
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  markSaved(current: OriginalDocumentEnvelope): void {
    this.begin();
    try {
      this.requireInitialized();
      this.requireCurrent(current);
      if (this.draft !== this.baseText) throw new Error('仍有未应用草稿，不能标记文档已保存');
      // UI may call ONLY after actual system save AND verified readback pass.
      // Native success alone, a cancelled picker or a partial write is not save.
      this.documentDirty = false;
    } catch (error) {
      this.recordFailure(error as Error);
      throw error;
    }
  }

  reset(): void {
    // UI may call ONLY following successful Close or confirmed replacement.
    this.begin();
    this.initialized = false;
    this.name = '';
    this.baseRevision = 0;
    this.baseSha256 = '';
    this.baseBytes = 0;
    this.baseText = '';
    this.draft = '';
    this.documentDirty = false;
    this.clearPrepared();
  }

  private begin(): void { this.sequence++; this.error = ''; }
  private recordFailure(error: Error): void { this.error = error.message || '原版文档编辑操作未完成'; }
  private requireInitialized(): void {
    if (!this.initialized) throw new Error('请先捕获已成功呈现的原生来源文档');
  }
  private requireCurrent(current: OriginalDocumentEnvelope): void {
    const checked: OriginalDocumentEnvelope = requirePresentedDocument(current);
    if (checked.core.name !== this.name || checked.core.revision !== this.baseRevision ||
      checked.core.sha256 !== this.baseSha256 || checked.core.byteCount !== this.baseBytes) {
      throw new Error('当前文档与编辑基线不一致；禁止用陈旧草稿覆盖新文档');
    }
  }
  private setBaseline(document: OriginalDocumentEnvelope, text: string): void {
    this.initialized = true;
    this.everInitialized = true;
    this.name = document.core.name;
    this.baseRevision = document.core.revision;
    this.baseSha256 = document.core.sha256;
    this.baseBytes = document.core.byteCount;
    this.baseText = text;
  }
  private clearPrepared(): void {
    this.preparedText = '';
    this.preparedBytes = new Uint8Array(0);
  }
}
