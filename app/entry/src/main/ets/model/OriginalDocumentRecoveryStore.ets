// App-private two-generation checkpoint IO only. Never derives paths from document/provider URIs.
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { OriginalDocumentRecoveryJournalPort, MAXIMUM_RECOVERY_FILE_BYTES } from './OriginalDocumentRecoveryJournal';
import { recoveryUtf8Bytes } from './OriginalDocumentRecovery';

interface RecoveryStoreIoError extends Error { code?: number; }

export class HarmonyOriginalDocumentRecoveryPort implements OriginalDocumentRecoveryJournalPort {
  private directory: string;
  constructor(directory: string) {
    // Caller supplies its actual context.filesDir, not a provider URI. Lexical
    // checks are not proof of ownership, symlink resolution or permissions.
    if (typeof directory !== 'string' || directory.length < 2 || directory.length > 4096 ||
      !directory.startsWith('/') || /[\x00-\x1F\x7F\\?#:]/.test(directory)) {
      throw new Error('应用私有恢复目录不可用');
    }
    const parts: string[] = directory.split('/');
    for (let i: number = 1; i < parts.length; i++) {
      if (parts[i] === '' || parts[i] === '.' || parts[i] === '..') throw new Error('应用私有恢复目录必须为固定绝对路径');
    }
    this.directory = directory;
  }
  readSlot(slot: number): string | undefined {
    const path: string = this.slotPath(slot);
    let file: fileIo.File;
    try {
      if (!fileIo.accessSync(path)) return undefined;
      file = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
    } catch (error) {
      // SDK declares both false and 13900002 for absence. Restrict this catch
      // to existence/open: read, decode and close failures stay failures.
      const failure: RecoveryStoreIoError = error as RecoveryStoreIoError;
      if (failure !== null && typeof failure === 'object' && failure.code === 13900002) return undefined;
      throw failure;
    }
    return this.readExact(file);
  }
  writeTemporary(text: string): void {
    const bytes: Uint8Array = recoveryUtf8Bytes(text);
    if (bytes.byteLength < 1 || bytes.byteLength > MAXIMUM_RECOVERY_FILE_BYTES) throw new Error('恢复副本超过限额');
    const file: fileIo.File = fileIo.openSync(this.temporaryPath(),
      fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
    let failure: Error | undefined = undefined;
    try {
      let at: number = 0;
      while (at < bytes.byteLength) {
        const chunk: Uint8Array = bytes.slice(at, Math.min(at + 64 * 1024, bytes.byteLength));
        const count: number = fileIo.writeSync(file.fd, chunk.buffer, { length: chunk.byteLength });
        if (!Number.isInteger(count) || count <= 0 || count > chunk.byteLength) throw new Error('恢复副本写入未完成');
        at += count;
      }
      fileIo.fsyncSync(file.fd);
    } catch (error) { failure = error as Error; }
    this.closeChecked(file, failure);
  }
  readTemporary(): string {
    return this.readExact(fileIo.openSync(this.temporaryPath(), fileIo.OpenMode.READ_ONLY));
  }
  promoteTemporary(slot: number): void { fileIo.renameSync(this.temporaryPath(), this.slotPath(slot)); }

  private slotPath(slot: number): string {
    if (slot !== 0 && slot !== 1) throw new Error('恢复副本编号不正确');
    return this.directory + '/original-document-recovery-' + slot + '.json';
  }
  private temporaryPath(): string { return this.directory + '/original-document-recovery.pending'; }
  private readExact(file: fileIo.File): string {
    let failure: Error | undefined = undefined;
    let text: string = '';
    try {
      const size: number = fileIo.statSync(file.fd).size;
      if (!Number.isSafeInteger(size) || size < 1 || size > MAXIMUM_RECOVERY_FILE_BYTES) throw new Error('恢复副本大小不合法或超过限额');
      const bytes: Uint8Array = new Uint8Array(size);
      let at: number = 0;
      while (at < size) {
        const chunk: ArrayBuffer = new ArrayBuffer(Math.min(64 * 1024, size - at));
        const count: number = fileIo.readSync(file.fd, chunk);
        if (!Number.isInteger(count) || count <= 0 || count > chunk.byteLength) throw new Error('恢复副本读取未完成');
        bytes.set(new Uint8Array(chunk, 0, count), at); at += count;
      }
      const extra: ArrayBuffer = new ArrayBuffer(1);
      if (fileIo.readSync(file.fd, extra) !== 0 || fileIo.statSync(file.fd).size !== size) throw new Error('恢复副本读取时大小发生变化');
      // Keep a leading BOM as text: decoding must not silently normalize bytes.
      const decoder: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: true, ignoreBOM: true });
      text = decoder.decodeToString(bytes, { stream: false });
    } catch (error) { failure = error as Error; }
    this.closeChecked(file, failure);
    return text;
  }
  private closeChecked(file: fileIo.File, failure: Error | undefined): void {
    try { fileIo.closeSync(file); }
    catch (error) {
      const closeFailure: Error = error as Error;
      if (failure !== undefined) {
        throw new Error(failure.message + '；恢复副本关闭失败：' + closeFailure.message);
      }
      throw closeFailure;
    }
    if (failure !== undefined) throw failure;
  }
}
