// Ordinary per-tab save metadata only. The caller records a target ONLY after
// its real write/readback transaction passed. No source/saved bytes are held,
// and validating a SHA-shaped string is not independent digest verification.
import {
  MAXIMUM_ORIGINAL_DOCUMENT_BYTES, originalDocumentFormat, originalDocumentNameFromUri,
  validateOriginalDocumentName
} from './OriginalDocumentTransfer';

export interface OriginalDocumentSaveTarget {
  id: string;
  uri: string;
  name: string;
  byteCount: number;
  sha256: string;
  sequence: number;
}

function validateSaveId(id: string): void {
  if (typeof id !== 'string' || id.length === 0 || id.length > 128 ||
    id.trim().length === 0 || /[\x00-\x1F\x7F]/.test(id)) {
    throw new Error('保存目标的标签 ID 必须为 1 至 128 个字符且不能仅为空白');
  }
}

function checkedSaveName(uri: string, nativeName: string): string {
  validateOriginalDocumentName(nativeName);
  // Require a file URI with an absolute provider path. Validate its lexical
  // form only: never turn it into a local path, normalize it or infer aliases.
  if (typeof uri !== 'string' || !uri.startsWith('file://') || uri.length > 16 * 1024 ||
    /[\x00-\x20\x7F\\<>"`{}|^]/.test(uri)) {
    throw new Error('请选择系统文件选择器提供的合法 file:// 文档 URI');
  }
  const location: string = uri.split('?')[0].split('#')[0];
  if (location.indexOf('/', 7) < 0) {
    throw new Error('文档 URI 必须包含完整的文件位置和名称');
  }
  try {
    decodeURI(uri);
  } catch (_) {
    throw new Error('文档 URI 的编码不完整');
  }
  const name: string = originalDocumentNameFromUri(uri);
  if (originalDocumentFormat(name) !== originalDocumentFormat(nativeName)) {
    throw new Error('另存文件的扩展名必须与原版保存格式一致');
  }
  return name;
}

function copySaveTarget(target: OriginalDocumentSaveTarget): OriginalDocumentSaveTarget {
  return { id: target.id, uri: target.uri, name: target.name, byteCount: target.byteCount,
    sha256: target.sha256, sequence: target.sequence };
}

export class OriginalDocumentSaveTargets {
  private maximumDocuments: number;
  private targets: OriginalDocumentSaveTarget[] = [];
  private sequence: number = 0;

  constructor(maximumDocuments: number = 4) {
    if (!Number.isSafeInteger(maximumDocuments) || maximumDocuments < 1 || maximumDocuments > 4) {
      throw new Error('保存目标数量上限必须为 1 至 4 的整数');
    }
    this.maximumDocuments = maximumDocuments;
  }

  get(id: string): OriginalDocumentSaveTarget | undefined {
    for (let i: number = 0; i < this.targets.length; i++) {
      if (this.targets[i].id === id) return copySaveTarget(this.targets[i]);
    }
    return undefined;
  }

  getSummaries(): OriginalDocumentSaveTarget[] {
    const summaries: OriginalDocumentSaveTarget[] = [];
    for (let i: number = 0; i < this.targets.length; i++) {
      summaries.push(copySaveTarget(this.targets[i]));
    }
    return summaries;
  }

  checkDestination(id: string, uri: string, nativeName: string): void {
    validateSaveId(id);
    checkedSaveName(uri, nativeName);
    for (let i: number = 0; i < this.targets.length; i++) {
      const target: OriginalDocumentSaveTarget = this.targets[i];
      if (target.id !== id && target.uri === uri) {
        throw new Error('该保存目标已属于另一个打开的标签，请另存为其他文档');
      }
    }
  }

  recordSaved(id: string, uri: string, nativeName: string, byteCount: number,
    sha256: string): OriginalDocumentSaveTarget {
    this.checkDestination(id, uri, nativeName);
    if (!Number.isSafeInteger(byteCount) || byteCount < 1 || byteCount > MAXIMUM_ORIGINAL_DOCUMENT_BYTES ||
      typeof sha256 !== 'string' || sha256.length !== 64 || !/^[0-9a-f]{64}$/.test(sha256)) {
      throw new Error('保存元数据必须含 1 字节至 8 MiB 的整数长度和 64 位小写 SHA256');
    }
    let index: number = -1;
    for (let i: number = 0; i < this.targets.length; i++) {
      if (this.targets[i].id === id) { index = i; break; }
    }
    if (index < 0 && this.targets.length >= this.maximumDocuments) {
      throw new Error('保存目标已达到标签数量上限');
    }
    const sequence: number = this.sequence + 1;
    if (!Number.isSafeInteger(sequence)) throw new Error('保存目标序号已达到上限');
    const target: OriginalDocumentSaveTarget = { id: id, uri: uri,
      name: originalDocumentNameFromUri(uri), byteCount: byteCount, sha256: sha256, sequence: sequence };
    // All checks finish before committing either the record or the counter.
    if (index < 0) this.targets.push(target);
    else this.targets[index] = target;
    this.sequence = sequence;
    return copySaveTarget(target);
  }

  restoreRemembered(target: OriginalDocumentSaveTarget): void {
    if (target === null || typeof target !== 'object' || Array.isArray(target)) {
      throw new Error('恢复保存元数据必须为对象');
    }
    const fields: string[] = Object.keys(target);
    const allowed: string[] = ['id', 'uri', 'name', 'byteCount', 'sha256', 'sequence'];
    if (fields.length !== allowed.length) throw new Error('恢复保存元数据字段不完整');
    for (let i: number = 0; i < fields.length; i++) {
      if (allowed.indexOf(fields[i]) < 0) throw new Error('恢复保存元数据含未知字段');
    }
    if (typeof target.name !== 'string') throw new Error('恢复保存目标名称必须为文本');
    this.checkDestination(target.id, target.uri, target.name);
    if (checkedSaveName(target.uri, target.name) !== target.name ||
      !Number.isSafeInteger(target.byteCount) || target.byteCount < 1 ||
      target.byteCount > MAXIMUM_ORIGINAL_DOCUMENT_BYTES || typeof target.sha256 !== 'string' ||
      !/^[0-9a-f]{64}$/.test(target.sha256) || !Number.isSafeInteger(target.sequence) || target.sequence < 1) {
      throw new Error('恢复保存元数据名称、长度、摘要或序号不合法');
    }
    for (let i: number = 0; i < this.targets.length; i++) {
      if (this.targets[i].id === target.id || this.targets[i].sequence === target.sequence) {
        throw new Error('恢复保存目标的标签或历史序号重复');
      }
    }
    if (this.targets.length >= this.maximumDocuments) throw new Error('恢复保存目标数量已达到上限');
    // Restoring metadata is not recordSaved and does not mint a write sequence.
    this.targets.push(copySaveTarget(target));
    this.sequence = Math.max(this.sequence, target.sequence);
  }

  retire(id: string): void {
    for (let i: number = 0; i < this.targets.length; i++) {
      if (this.targets[i].id === id) { this.targets.splice(i, 1); return; }
    }
  }
}
