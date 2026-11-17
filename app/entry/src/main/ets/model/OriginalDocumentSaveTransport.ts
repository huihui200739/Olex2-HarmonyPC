// A transaction-scoped wrapper around the real platform backend/port. Only
// snapshot length/digest and an independent target metadata copy are retained.
// A captured snapshot is NOT a successful save; only a passed real transfer is.
import {
  OriginalDocumentBackend, OriginalDocumentPort, originalDocumentFormat,
  originalDocumentNameFromUri, validateOriginalDocumentBytes, validateOriginalDocumentName
} from './OriginalDocumentTransfer';
import { OriginalDocumentSaveTarget } from './OriginalDocumentSaveTargets';

export interface OriginalDocumentSerializedSnapshot {
  byteCount: number;
  sha256: string;
}

function copySaveBytes(bytes: Uint8Array): Uint8Array {
  const copy: Uint8Array = new Uint8Array(bytes.byteLength);
  copy.set(bytes);
  return copy;
}

export class OriginalDocumentSaveTransport implements OriginalDocumentBackend, OriginalDocumentPort {
  private backend: OriginalDocumentBackend;
  private port: OriginalDocumentPort;
  private digest: (bytes: Uint8Array) => string;
  private checkWrite: (uri: string) => void;
  private target: OriginalDocumentSaveTarget | undefined;
  private reuseTarget: boolean;
  private captured: OriginalDocumentSerializedSnapshot | undefined;
  private startedWrite: boolean = false;

  constructor(backend: OriginalDocumentBackend, port: OriginalDocumentPort,
    digest: (bytes: Uint8Array) => string, checkWrite: (uri: string) => void,
    target?: OriginalDocumentSaveTarget, reuseTarget: boolean = false) {
    this.backend = backend;
    this.port = port;
    this.digest = digest;
    this.checkWrite = checkWrite;
    this.reuseTarget = reuseTarget;
    if (target !== undefined) {
      this.target = { id: target.id, uri: target.uri, name: target.name, byteCount: target.byteCount,
        sha256: target.sha256, sequence: target.sequence };
    }
  }

  open(name: string, bytes: Uint8Array): string { return this.backend.open(name, bytes); }
  close(): string { return this.backend.close(); }
  status(): string { return this.backend.status(); }
  selectImport(): Promise<string[]> { return this.port.selectImport(); }
  read(uri: string): Uint8Array { return this.port.read(uri); }

  snapshot(): Uint8Array {
    // A failed new capture must not leave a previous snapshot writeable.
    this.startedWrite = false;
    this.captured = undefined;
    const bytes: Uint8Array = copySaveBytes(this.backend.snapshot());
    validateOriginalDocumentBytes(bytes);
    const sha256: string = this.checkedDigest(bytes);
    this.captured = { byteCount: bytes.byteLength, sha256: sha256 };
    return bytes;
  }

  getSnapshot(): OriginalDocumentSerializedSnapshot | undefined {
    const captured: OriginalDocumentSerializedSnapshot | undefined = this.captured;
    if (captured === undefined) return undefined;
    return { byteCount: captured.byteCount, sha256: captured.sha256 };
  }

  selectExport(name: string): Promise<string[]> {
    validateOriginalDocumentName(name);
    const target: OriginalDocumentSaveTarget | undefined = this.target;
    if (this.reuseTarget && target !== undefined) {
      validateOriginalDocumentName(target.name);
      const savedName: string = originalDocumentNameFromUri(target.uri);
      if (originalDocumentFormat(savedName) !== originalDocumentFormat(name) ||
        originalDocumentFormat(target.name) !== originalDocumentFormat(name)) {
        throw new Error('保存目标的扩展名与当前原版保存格式不一致，请另存为');
      }
      return Promise.resolve([target.uri]);
    }
    let suggestedName: string = name;
    if (target !== undefined && originalDocumentFormat(target.name) === originalDocumentFormat(name)) {
      validateOriginalDocumentName(target.name);
      suggestedName = target.name;
    }
    return this.port.selectExport(suggestedName);
  }

  writeVerified(uri: string, bytes: Uint8Array): void {
    // Observation belongs to this attempt, including retries after a failure.
    this.startedWrite = false;
    const captured: OriginalDocumentSerializedSnapshot | undefined = this.captured;
    if (captured === undefined) throw new Error('请先获取原版保存快照');
    const data: Uint8Array = copySaveBytes(bytes);
    validateOriginalDocumentBytes(data);
    if (data.byteLength !== captured.byteCount || this.checkedDigest(data) !== captured.sha256) {
      throw new Error('写入内容与本次原版保存快照不一致');
    }
    const target: OriginalDocumentSaveTarget | undefined = this.target;
    if (target !== undefined && uri === target.uri) {
      // Read through the real port immediately before overwriting, even if a
      // picker explicitly selected the same URI. Do not infer URI aliases.
      try {
        const previous: Uint8Array = this.port.read(uri);
        validateOriginalDocumentBytes(previous);
        if (previous.byteLength !== target.byteCount || this.checkedDigest(previous) !== target.sha256) {
          throw new Error('已保存目标内容已改变');
        }
      } catch (_) {
        throw new Error('已保存目标内容已改变或无法读取，请使用“另存为”保存到其他文档');
      }
    }
    // The caller rechecks native ownership and cross-tab destination collisions
    // here, after picker/read callbacks and before the first real write call.
    this.checkWrite(uri);
    this.startedWrite = true;
    this.port.writeVerified(uri, data);
  }

  didStartWrite(): boolean { return this.startedWrite; }

  private checkedDigest(bytes: Uint8Array): string {
    const sha256: string = this.digest(bytes);
    if (typeof sha256 !== 'string' || sha256.length !== 64 || !/^[0-9a-f]{64}$/.test(sha256)) {
      throw new Error('保存摘要必须为 64 位小写 SHA256');
    }
    return sha256;
  }
}
