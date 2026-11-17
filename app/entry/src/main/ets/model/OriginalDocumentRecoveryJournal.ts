// Ordinary app-private checkpoint journal. No native calls, script execution or external-document writes.
import { OriginalDocumentRecoverySnapshot, validateOriginalDocumentRecovery,
  encodeOriginalDocumentRecovery, decodeOriginalDocumentRecovery, recoveryUtf8Bytes } from './OriginalDocumentRecovery';

export const MAXIMUM_RECOVERY_FILE_BYTES: number = 256 * 1024 * 1024;
export interface OriginalDocumentRecoveryJournalPort {
  readSlot(slot: number): string | undefined;
  writeTemporary(text: string): void;
  readTemporary(): string;
  promoteTemporary(slot: number): void;
}
export interface OriginalDocumentRecoveryJournalStatus {
  state: string; generation: number; selectedSlot: number; documentCount: number;
  savedAt: string; byteCount: number; sha256: string; error: string; warning: string;
  // Attempts are cumulative for this instance, including failed writes and explicit reloads.
  // Slot counts describe the latest reads, not write totals or a durability guarantee.
  writeSequence: number; validSlots: number; invalidSlots: number;
}
// snapshot is normalized schema 2; text remains the exact schema 1/2 slot bytes read.
interface RecoverySlot {
  slot: number; snapshot: OriginalDocumentRecoverySnapshot; text: string;
}

export class OriginalDocumentRecoveryJournal {
  private port: OriginalDocumentRecoveryJournalPort;
  private digest: (bytes: Uint8Array) => string;
  private snapshot: OriginalDocumentRecoverySnapshot | undefined = undefined;
  private loaded: boolean = false;
  private conflicted: boolean = false;
  // 0 = absent, 1 = decoded and valid, 2 = unreadable or invalid. Never infer validity from a rename.
  private slotStates: number[] = [0, 0];
  private slotErrors: string[] = ['', ''];
  private status: OriginalDocumentRecoveryJournalStatus = { state: 'not-loaded', generation: 0,
    selectedSlot: -1, documentCount: 0, savedAt: '', byteCount: 0, sha256: '', error: '', warning: '',
    writeSequence: 0, validSlots: 0, invalidSlots: 0 };

  constructor(port: OriginalDocumentRecoveryJournalPort, digest: (bytes: Uint8Array) => string) {
    this.port = port; this.digest = digest;
  }

  getStatus(): OriginalDocumentRecoveryJournalStatus {
    const s: OriginalDocumentRecoveryJournalStatus = this.status;
    return { state: s.state, generation: s.generation, selectedSlot: s.selectedSlot, documentCount: s.documentCount,
      savedAt: s.savedAt, byteCount: s.byteCount, sha256: s.sha256, error: s.error, warning: s.warning,
      writeSequence: s.writeSequence, validSlots: s.validSlots, invalidSlots: s.invalidSlots };
  }

  getSnapshot(): OriginalDocumentRecoverySnapshot | undefined {
    return this.snapshot === undefined ? undefined : validateOriginalDocumentRecovery(this.snapshot, this.digest);
  }

  load(): OriginalDocumentRecoverySnapshot | undefined {
    const slots: RecoverySlot[] = [];
    const attempts: number = this.status.writeSequence;
    this.snapshot = undefined; this.loaded = true; this.conflicted = false;
    this.slotStates = [0, 0]; this.slotErrors = ['', ''];
    this.status = { state: 'missing', generation: 0, selectedSlot: -1, documentCount: 0,
      savedAt: '', byteCount: 0, sha256: '', error: '', warning: '', writeSequence: attempts, validSlots: 0, invalidSlots: 0 };
    for (let slot: number = 0; slot < 2; slot++) {
      const observed: RecoverySlot | undefined = this.observeSlot(slot);
      if (observed !== undefined) {
        slots.push(observed);
        this.status.generation = Math.max(this.status.generation, observed.snapshot.generation);
      }
    }
    this.updateSlotStatus();
    if (slots.length === 0) {
      if (this.status.invalidSlots > 0) {
        this.status.state = 'invalid'; this.status.error = '没有取得完整可用的恢复副本；原文件保持，未自动覆盖。';
      }
      return undefined;
    }
    let chosen: RecoverySlot = slots[0];
    if (slots.length === 2) {
      if (slots[0].snapshot.generation === slots[1].snapshot.generation &&
        JSON.stringify(slots[0].snapshot) !== JSON.stringify(slots[1].snapshot)) {
        this.conflicted = true;
        this.status.state = 'conflict'; this.status.error = '两份恢复副本序号相同但内容不同；没有猜测或自动载入。';
        return undefined;
      }
      if (slots[1].snapshot.generation > chosen.snapshot.generation) chosen = slots[1];
    }
    this.accept(chosen.slot, chosen.snapshot, chosen.text, chosen.snapshot.documents.length > 0 ? 'available' : 'empty');
    return this.getSnapshot();
  }

  // Only the caller's explicit "do not recover; start a new session" confirmation may use this API.
  // With no selected candidate (conflict), write() deterministically targets slot 0 and preserves slot 1.
  startEmptyConfirmed(snapshot: OriginalDocumentRecoverySnapshot): OriginalDocumentRecoverySnapshot {
    if (!this.loaded) this.load();
    const wasConflicted: boolean = this.conflicted;
    try {
      const next: OriginalDocumentRecoverySnapshot = validateOriginalDocumentRecovery(snapshot, this.digest);
      if (next.documents.length !== 0 || next.activeId !== '') {
        throw new Error('确认开始新会话只允许空文档列表和空活动 ID，不能借此恢复任一冲突副本');
      }
      if (!Number.isSafeInteger(this.status.generation + 1) || next.generation !== this.status.generation + 1) {
        throw new Error('确认开始新会话必须使用下一个安全递增序号');
      }
      // Clear the guard only after full validation. write() still performs its normal transaction checks.
      this.conflicted = false;
      return this.write(next);
    } catch (error) {
      this.conflicted = wasConflicted;
      throw error;
    }
  }

  write(snapshot: OriginalDocumentRecoverySnapshot): OriginalDocumentRecoverySnapshot {
    this.status.writeSequence++;
    let target: number = -1;
    let promotionAttempted: boolean = false;
    let targetObserved: boolean = false;
    try {
      // Cold callers must observe both generations before choosing a target or validating the next number.
      if (!this.loaded) this.load();
      // A failed write must not clear this guard. Only load or startEmptyConfirmed may resolve a conflict.
      if (this.conflicted) throw new Error('恢复副本仍有同代冲突；必须先明确处理并重新载入，不能猜测覆盖任一副本');
      // Validation normalizes legacy input to 2; every NEW promotion uses a schema-2 envelope/payload.
      const next: OriginalDocumentRecoverySnapshot = validateOriginalDocumentRecovery(snapshot, this.digest);
      if (!Number.isSafeInteger(this.status.generation + 1) || next.generation !== this.status.generation + 1) {
        throw new Error('恢复备份必须使用下一个安全递增序号，不能覆盖或跳过当前代次');
      }
      const text: string = encodeOriginalDocumentRecovery(next, this.digest);
      this.requireBounded(text);
      target = this.status.selectedSlot === 0 ? 1 : 0;
      // The selected last-valid slot is never truncated. Only the app-owned temporary is written.
      this.port.writeTemporary(text);
      if (this.port.readTemporary() !== text) throw new Error('临时恢复副本逐字节回读未通过');
      promotionAttempted = true;
      this.port.promoteTemporary(target);
      const observed: RecoverySlot | undefined = this.observeSlot(target);
      targetObserved = true;
      this.updateSlotStatus();
      if (observed === undefined || observed.text !== text) {
        throw new Error('已替换的恢复副本回读未通过；上一份有效副本仍保留');
      }
      // Prepare the independent return copy before committing any accepted metadata.
      const result: OriginalDocumentRecoverySnapshot = validateOriginalDocumentRecovery(observed.snapshot, this.digest);
      this.accept(target, observed.snapshot, observed.text, observed.snapshot.documents.length > 0 ? 'saved' : 'empty');
      return result;
    } catch (error) {
      // A port may report a rename failure after touching its target. Re-read ONLY that non-selected slot.
      if (promotionAttempted && !targetObserved) this.observeSlot(target);
      this.updateSlotStatus();
      this.status.state = 'write-failed'; this.status.error = (error as Error).message || '恢复备份未确认完成';
      throw error;
    }
  }

  private observeSlot(slot: number): RecoverySlot | undefined {
    try {
      const text: string | undefined = this.port.readSlot(slot);
      if (text === undefined) {
        this.slotStates[slot] = 0; this.slotErrors[slot] = ''; return undefined;
      }
      this.requireBounded(text);
      // Decode accepts old schema 1 without inventing a viewport or modifying its original slot.
      const snapshot: OriginalDocumentRecoverySnapshot = decodeOriginalDocumentRecovery(text, this.digest);
      this.slotStates[slot] = 1; this.slotErrors[slot] = '';
      return { slot: slot, snapshot: snapshot, text: text };
    } catch (error) {
      this.slotStates[slot] = 2;
      this.slotErrors[slot] = `副本 ${slot + 1}：${(error as Error).message || '读取或完整性核对失败'}`;
      return undefined;
    }
  }

  private updateSlotStatus(): void {
    let valid: number = 0;
    let invalid: number = 0;
    const errors: string[] = [];
    for (let slot: number = 0; slot < 2; slot++) {
      if (this.slotStates[slot] === 1) valid++;
      else if (this.slotStates[slot] === 2) invalid++;
      if (this.slotErrors[slot].length > 0) errors.push(this.slotErrors[slot]);
    }
    this.status.validSlots = valid; this.status.invalidSlots = invalid;
    this.status.warning = errors.join('；');
  }

  private requireBounded(text: string): void {
    if (typeof text !== 'string' || text.length === 0 || text.length > MAXIMUM_RECOVERY_FILE_BYTES ||
      recoveryUtf8Bytes(text).byteLength > MAXIMUM_RECOVERY_FILE_BYTES) {
      throw new Error('恢复副本为空或超过 256 MiB 限额，没有替换上一份有效备份');
    }
  }

  private accept(slot: number, snapshot: OriginalDocumentRecoverySnapshot, text: string, state: string): void {
    const checked: OriginalDocumentRecoverySnapshot = validateOriginalDocumentRecovery(snapshot, this.digest);
    const bytes: Uint8Array = recoveryUtf8Bytes(text);
    const sha256: string = this.digest(bytes);
    this.snapshot = checked;
    this.status.state = state; this.status.generation = checked.generation; this.status.selectedSlot = slot;
    this.status.documentCount = checked.documents.length; this.status.savedAt = checked.savedAt;
    this.status.byteCount = bytes.byteLength; this.status.sha256 = sha256; this.status.error = '';
  }
}
