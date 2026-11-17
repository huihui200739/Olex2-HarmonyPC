// Bounded ordinary document/tab metadata and actual-source byte cache only.
// Caller supplies native SHA-checked source bytes after real load/apply. This
// pure registry validates metadata/UTF-8/length, NOT the SHA digest itself.
import {
  MAXIMUM_ORIGINAL_DOCUMENT_BYTES, OriginalDocumentCoreStatus, OriginalDocumentEnvelope,
  originalDocumentPresented, parseOriginalDocument, validateOriginalDocumentBytes
} from './OriginalDocumentTransfer';

export interface OriginalDocumentSessionSummary {
  id: string; name: string; sourceKind: string; sourceSha256: string; sourceBytes: number;
  nativeRevision: number; draftDirty: boolean; documentDirty: boolean; active: boolean; nativeBound: boolean;
}
export interface OriginalDocumentSessionsStatus {
  count: number; activeId: string; nativeSessionId: string; pendingId: string; totalSourceBytes: number;
  maximumDocuments: number; maximumSourceBytes: number; sequence: number; error: string;
}
export interface OriginalDocumentActivation {
  id: string; token: number; name: string; bytes: Uint8Array; sourceSha256: string; previousNativeRevision: number;
}
interface OriginalDocumentSessionRecord {
  id: string; sourceKind: string; document: OriginalDocumentEnvelope; bytes: Uint8Array;
  identity: string; generation: number; draftDirty: boolean; documentDirty: boolean;
}
interface OriginalDocumentPendingActivation {
  id: string; token: number; name: string; sourceSha256: string; byteCount: number;
  previousNativeRevision: number; generation: number;
  oldActiveId: string; oldNativeSessionId: string; oldIdentity: string;
}
// Native retention is ownership state, not a rendering/selection observation.
interface OriginalDocumentSessionOwnerCore extends OriginalDocumentCoreStatus {
  contextRetained?: boolean;
}
interface OriginalDocumentStableIdentity {
  passed: boolean; surfacePresented: boolean; parentContextRestored: boolean; error: string;
  sessionRetained?: boolean; contextRetained?: boolean;
  corePassed: boolean; loaded: boolean; name: string; format: string; byteCount: number;
  sha256: string; revision: number; source: string; atomCount: number; bondCount: number;
  coreError: string; modelMayHaveChanged: boolean;
}

function checkedPresented(document: OriginalDocumentEnvelope): OriginalDocumentEnvelope {
  const checked: OriginalDocumentEnvelope = parseOriginalDocument(JSON.stringify(document));
  if (!originalDocumentPresented(checked)) throw new Error('文档尚未完整成功载入并呈现');
  const core: OriginalDocumentSessionOwnerCore = checked.core as OriginalDocumentSessionOwnerCore;
  // Legacy envelopes may omit retention; if supplied, successful ownership
  // requires true, never false, a truthy string or another malformed value.
  if (checked.sessionRetained === false || (core.contextRetained !== undefined && core.contextRetained !== true)) {
    throw new Error('已呈现文档未保留原生会话或上下文');
  }
  return checked;
}
function checkedSource(document: OriginalDocumentEnvelope, bytes: Uint8Array): OriginalDocumentEnvelope {
  const checked: OriginalDocumentEnvelope = checkedPresented(document);
  if (!(bytes instanceof Uint8Array)) throw new Error('来源内容必须为 UTF-8 字节');
  validateOriginalDocumentBytes(bytes);
  if (checked.core.byteCount !== bytes.byteLength) throw new Error('来源字节数与文档状态不一致');
  return checked;
}
function copySource(bytes: Uint8Array): Uint8Array {
  // Allocate our own plain storage, including for incoming offset views. Do not
  // delegate ownership to an input object's potentially view-returning slice.
  const copy: Uint8Array = new Uint8Array(bytes.byteLength);
  copy.set(bytes);
  return copy;
}
function equalSource(left: Uint8Array, right: Uint8Array): boolean {
  if (left.byteLength !== right.byteLength) return false;
  for (let i: number = 0; i < left.byteLength; i++) {
    if (left[i] !== right[i]) return false;
  }
  return true;
}
function presentedIdentity(document: OriginalDocumentEnvelope): string {
  // Every caller first passes checkedPresented/checkedSource. Keep complete
  // native presentation validation, then compare ONLY explicit stable source
  // and ownership state, never all Object.keys of a changing status response.
  // Native Status in original_document_view.cpp exports these transient fields:
  // frames, exportByteCount; selectedObjectCount, selectionSequence, pickHit,
  // lastPickedCollection, selectionAction, originalPickingCalls, lastPickPixelX,
  // lastPickPixelY; viewZoom, viewCenter; sampleWidth, sampleHeight,
  // sampleNonBackgroundPixels; visibleSites, renderObjects, renderCollections,
  // renderPrimitives, sceneComplete. They do not acquire/replace source owner.
  // selectionSource is optional observation provenance, not owner identity;
  // parseOriginalDocument still requires its exact known value when present.
  // Future unknown observation keys cannot silently become ownership identity.
  const core: OriginalDocumentSessionOwnerCore = document.core as OriginalDocumentSessionOwnerCore;
  const identity: OriginalDocumentStableIdentity = {
    passed: document.passed, surfacePresented: document.surfacePresented,
    parentContextRestored: document.parentContextRestored, error: document.error,
    sessionRetained: document.sessionRetained, contextRetained: core.contextRetained,
    corePassed: core.passed, loaded: core.loaded, name: core.name, format: core.format,
    byteCount: core.byteCount, sha256: core.sha256, revision: core.revision, source: core.source,
    atomCount: core.atomCount, bondCount: core.bondCount, coreError: core.error,
    modelMayHaveChanged: core.modelMayHaveChanged
  };
  return JSON.stringify(identity);
}

export class OriginalDocumentSessions {
  private sessions: OriginalDocumentSessionRecord[] = [];
  private activeId: string = '';
  private nativeSessionId: string = '';
  private pending: OriginalDocumentPendingActivation | undefined = undefined;
  private totalSourceBytes: number = 0;
  private maximumDocuments: number;
  private maximumSourceBytes: number;
  private nextId: number = 1;
  private nextToken: number = 1;
  private sequence: number = 0;
  private error: string = '';

  constructor(maximumDocuments: number = 4, maximumSourceBytes: number = 32 * 1024 * 1024) {
    if (!Number.isSafeInteger(maximumDocuments) || maximumDocuments < 1 || maximumDocuments > 4 ||
      !Number.isSafeInteger(maximumSourceBytes) || maximumSourceBytes < 1 || maximumSourceBytes > 32 * 1024 * 1024) {
      throw new Error('会话上限必须为 1 至 4 个文档，总来源容量为 1 字节至 32 MiB');
    }
    this.maximumDocuments = maximumDocuments;
    this.maximumSourceBytes = maximumSourceBytes;
  }

  getStatus(): OriginalDocumentSessionsStatus {
    return {
      count: this.sessions.length, activeId: this.activeId, nativeSessionId: this.nativeSessionId,
      pendingId: this.pending === undefined ? '' : this.pending.id, totalSourceBytes: this.totalSourceBytes,
      maximumDocuments: this.maximumDocuments, maximumSourceBytes: this.maximumSourceBytes,
      sequence: this.sequence, error: this.error
    };
  }

  getSummaries(): OriginalDocumentSessionSummary[] {
    const summaries: OriginalDocumentSessionSummary[] = [];
    for (const session of this.sessions) {
      summaries.push({
        id: session.id, name: session.document.core.name, sourceKind: session.sourceKind,
        sourceSha256: session.document.core.sha256, sourceBytes: session.bytes.byteLength,
        nativeRevision: session.document.core.revision, draftDirty: session.draftDirty,
        documentDirty: session.documentDirty, active: session.id === this.activeId,
        nativeBound: session.id === this.activeId && session.id === this.nativeSessionId
      });
    }
    return summaries;
  }

  canAdd(byteCount: number = MAXIMUM_ORIGINAL_DOCUMENT_BYTES): boolean {
    return this.pending === undefined && Number.isSafeInteger(byteCount) && byteCount >= 1 &&
      byteCount <= MAXIMUM_ORIGINAL_DOCUMENT_BYTES && this.sessions.length < this.maximumDocuments &&
      this.totalSourceBytes + byteCount <= this.maximumSourceBytes;
  }

  add(document: OriginalDocumentEnvelope, bytes: Uint8Array, sourceKind: string): string {
    this.requireIdle();
    const checked: OriginalDocumentEnvelope = this.validateSource(document, bytes);
    if (typeof sourceKind !== 'string' || sourceKind.trim().length === 0) throw this.failure('来源类型不能为空');
    if (!this.canAdd(bytes.byteLength)) throw this.failure('文档数量或来源容量已达到上限');
    if (!Number.isSafeInteger(this.nextId + 1)) throw this.failure('文档编号已达到上限');
    const id: string = 'document-' + this.nextId;
    const record: OriginalDocumentSessionRecord = {
      id: id, sourceKind: sourceKind, document: checked, bytes: copySource(bytes),
      identity: presentedIdentity(checked), generation: 1, draftDirty: false, documentDirty: false
    };
    this.sessions.push(record);
    this.nextId++;
    this.totalSourceBytes += record.bytes.byteLength;
    // add is a COMMIT after caller-observed successful real load/source, not a
    // reservation and never a picker, native call or file-open substitute.
    this.activeId = id;
    this.nativeSessionId = id;
    this.changed();
    return id;
  }

  getCachedSource(id: string): Uint8Array {
    for (let i: number = 0; i < this.sessions.length; i++) {
      if (this.sessions[i].id === id) return copySource(this.sessions[i].bytes);
    }
    // A read-only lookup must not mutate even the registry's error/status.
    throw new Error('文档会话不存在');
  }

  restorePresented(id: string, actualPresentedEnvelope: OriginalDocumentEnvelope,
    actualNativeSourceBytes: Uint8Array, sourceKind: string, draftDirty: boolean,
    documentDirty: boolean): void {
    this.requireIdle();
    // Caller must first perform a REAL successful native load and obtain its
    // SHA-checked source. A persisted envelope/revision is never an input here.
    if (typeof id !== 'string' || id.length > 25 || !/^document-[1-9][0-9]*$/.test(id)) {
      throw this.failure('恢复标签 ID 必须为规范 document-N');
    }
    const number: number = Number(id.substring(9));
    if (!Number.isSafeInteger(number) || !Number.isSafeInteger(number + 1)) {
      throw this.failure('恢复标签编号无法安全递增');
    }
    if (typeof sourceKind !== 'string' || sourceKind.length > 128 ||
      sourceKind.trim().length === 0 || /[\x00-\x1F\x7F]/.test(sourceKind) ||
      typeof draftDirty !== 'boolean' || typeof documentDirty !== 'boolean') {
      throw this.failure('恢复来源类型或脏状态不合法');
    }
    for (let i: number = 0; i < this.sessions.length; i++) {
      if (this.sessions[i].id === id) throw this.failure('恢复标签 ID 重复');
    }
    const checked: OriginalDocumentEnvelope = this.validateSource(actualPresentedEnvelope, actualNativeSourceBytes);
    if (!this.canAdd(actualNativeSourceBytes.byteLength)) throw this.failure('恢复文档数量或来源容量已达到上限');
    const record: OriginalDocumentSessionRecord = {
      id: id, sourceKind: sourceKind, document: checked, bytes: copySource(actualNativeSourceBytes),
      identity: presentedIdentity(checked), generation: 1, draftDirty: draftDirty, documentDirty: documentDirty
    };
    this.sessions.push(record);
    this.nextId = Math.max(this.nextId, number + 1);
    this.totalSourceBytes += record.bytes.byteLength;
    this.activeId = id;
    this.nativeSessionId = id;
    this.changed();
  }

  setDirty(id: string, draftDirty: boolean, documentDirty: boolean): void {
    const session: OriginalDocumentSessionRecord = this.requireSession(id);
    if (typeof draftDirty !== 'boolean' || typeof documentDirty !== 'boolean') throw this.failure('脏状态必须为布尔值');
    if (session.draftDirty === draftDirty && session.documentDirty === documentDirty) return;
    session.draftDirty = draftDirty;
    session.documentDirty = documentDirty;
    this.changed();
  }

  // A successful original command can change the live model (for example pack
  // adds atoms) without touching the source bytes or revision. Re-derive the
  // presented identity for the active source so the session binding survives the
  // model edit; mismatched or unloaded sources are left to the normal reconcile.
  refreshPresentedIdentity(document: OriginalDocumentEnvelope): void {
    if (this.activeId === '' || this.nativeSessionId !== this.activeId) return;
    let checked: OriginalDocumentEnvelope;
    try { checked = parseOriginalDocument(JSON.stringify(document)); } catch (_) { return; }
    if (!originalDocumentPresented(checked)) return;
    const session: OriginalDocumentSessionRecord = this.requireSession(this.activeId);
    if (checked.core.name !== session.document.core.name || checked.core.sha256 !== session.document.core.sha256 ||
      checked.core.byteCount !== session.document.core.byteCount ||
      checked.core.revision !== session.document.core.revision) return;
    const identity: string = presentedIdentity(checked);
    if (identity === session.identity) return;
    session.document = checked;
    session.identity = identity;
    this.changed();
  }

  updateApplied(id: string, document: OriginalDocumentEnvelope, bytes: Uint8Array): void {
    this.requireIdle();
    const session: OriginalDocumentSessionRecord = this.requireBound(id);
    const checked: OriginalDocumentEnvelope = this.validateSource(document, bytes);
    if (checked.core.name !== session.document.core.name || checked.core.revision <= session.document.core.revision) {
      throw this.failure('应用结果名称不符或 revision 未递增');
    }
    const total: number = this.totalSourceBytes - session.bytes.byteLength + bytes.byteLength;
    if (total > this.maximumSourceBytes) throw this.failure('应用后来源容量将超过上限');
    const copy: Uint8Array = copySource(bytes);
    const identity: string = presentedIdentity(checked);
    session.document = checked;
    session.bytes = copy;
    session.identity = identity;
    session.generation++;
    session.documentDirty = true;
    // Do not retire an editor's pending draft here. Parent accepts its applied
    // transaction separately, then synchronizes draftDirty with setDirty.
    this.totalSourceBytes = total;
    this.changed();
  }

  markSaved(id: string): void {
    this.requireIdle();
    const session: OriginalDocumentSessionRecord = this.requireBound(id);
    if (session.draftDirty) throw this.failure('仍有未应用草稿，不能标记已保存');
    // Caller only after actual external save AND verified readback, not cancel.
    if (!session.documentDirty) return;
    session.documentDirty = false;
    this.changed();
  }

  beginActivation(id: string): OriginalDocumentActivation {
    this.requireIdle();
    const session: OriginalDocumentSessionRecord = this.requireSession(id);
    if (id === this.activeId && id === this.nativeSessionId) throw this.failure('当前文档已绑定原生会话');
    if (!Number.isSafeInteger(this.nextToken + 1)) throw this.failure('切换编号已达到上限');
    let oldIdentity: string = '';
    if (this.nativeSessionId !== '') oldIdentity = this.requireSession(this.nativeSessionId).identity;
    const request: OriginalDocumentActivation = {
      id: id, token: this.nextToken, name: session.document.core.name, bytes: copySource(session.bytes),
      sourceSha256: session.document.core.sha256, previousNativeRevision: session.document.core.revision
    };
    this.pending = {
      id: id, token: this.nextToken, name: request.name, sourceSha256: request.sourceSha256,
      byteCount: session.bytes.byteLength, previousNativeRevision: request.previousNativeRevision,
      generation: session.generation, oldActiveId: this.activeId,
      oldNativeSessionId: this.nativeSessionId, oldIdentity: oldIdentity
    };
    this.nextToken++;
    this.changed();
    return request;
  }

  acceptActivation(request: OriginalDocumentActivation, document: OriginalDocumentEnvelope, bytes: Uint8Array): void {
    const pending: OriginalDocumentPendingActivation = this.requireTicket(request);
    const session: OriginalDocumentSessionRecord = this.requireSession(pending.id);
    if (request.name !== pending.name || request.sourceSha256 !== pending.sourceSha256 ||
      request.previousNativeRevision !== pending.previousNativeRevision || !(request.bytes instanceof Uint8Array) ||
      !equalSource(request.bytes, session.bytes) || session.generation !== pending.generation ||
      session.document.core.name !== pending.name || session.document.core.sha256 !== pending.sourceSha256 ||
      session.bytes.byteLength !== pending.byteCount || session.document.core.revision !== pending.previousNativeRevision) {
      throw this.failure('切换请求或缓存来源已改变');
    }
    const checked: OriginalDocumentEnvelope = this.validateSource(document, bytes);
    if (checked.core.name !== pending.name || checked.core.sha256 !== pending.sourceSha256 ||
      checked.core.byteCount !== pending.byteCount || checked.core.revision <= pending.previousNativeRevision ||
      !equalSource(bytes, session.bytes)) {
      throw this.failure('切换结果与目标实际来源或 revision 不一致');
    }
    const identity: string = presentedIdentity(checked);
    session.document = checked;
    session.identity = identity;
    this.activeId = session.id;
    this.nativeSessionId = session.id;
    this.pending = undefined;
    this.changed();
  }

  abortActivation(request: OriginalDocumentActivation, observed?: OriginalDocumentEnvelope): void {
    const pending: OriginalDocumentPendingActivation = this.requireTicket(request);
    let preserve: boolean = false;
    if (observed !== undefined && pending.oldNativeSessionId !== '' &&
      this.nativeSessionId === pending.oldNativeSessionId && this.activeId === pending.oldActiveId) {
      try { preserve = presentedIdentity(checkedPresented(observed)) === pending.oldIdentity; }
      catch (_) { preserve = false; }
    }
    // Abort is cleanup by token/id even if the caller mutated request payload.
    // Never manufacture a rollback; invalidation cannot be undone by an abort.
    if (!preserve) this.nativeSessionId = '';
    this.pending = undefined;
    this.changed();
  }

  invalidateNative(): void {
    if (this.nativeSessionId === '') return;
    this.nativeSessionId = '';
    // A pending ticket remains abortable/acceptable but cannot revive the old
    // binding through abort, even if passed a formerly matching old observation.
    this.changed();
  }

  matchesNative(document: OriginalDocumentEnvelope): boolean {
    if (this.activeId === '' || this.nativeSessionId !== this.activeId) return false;
    try {
      const session: OriginalDocumentSessionRecord = this.requireSession(this.activeId);
      return presentedIdentity(checkedPresented(document)) === session.identity;
    } catch (_) { return false; }
  }
  // An ordinary main-frame action can fail (for example the original grid handler
  // refusing a fixture without a crystallographic map) while the same source is
  // still loaded in the native session. That failure must not invalidate the
  // session ownership binding. Compare only the stable source/ownership fields,
  // never the operation outcome or transient observation fields, and fail closed
  // when the document is no longer loaded or was replaced/modified.
  // Style-only changes (bond radius, drawing settings) can flip
  // modelMayHaveChanged without changing the source or revision. The original
  // application keeps such a document open; keep the ownership binding and let
  // the caller refresh the presented identity.
  matchesNativeSourceRevision(document: OriginalDocumentEnvelope): boolean {
    if (this.activeId === '' || this.nativeSessionId !== this.activeId) return false;
    try {
      const session: OriginalDocumentSessionRecord = this.requireSession(this.activeId);
      const stored: OriginalDocumentStableIdentity = JSON.parse(session.identity) as OriginalDocumentStableIdentity;
      const core = document.core;
      return core.loaded === true &&
        core.name === stored.name && core.format === stored.format &&
        core.byteCount === stored.byteCount && core.sha256 === stored.sha256 &&
        core.revision === stored.revision && core.source === stored.source;
    } catch (_) { return false; }
  }

  matchesNativeSourceIdentity(document: OriginalDocumentEnvelope): boolean {
    if (this.activeId === '' || this.nativeSessionId !== this.activeId) return false;
    try {
      const session: OriginalDocumentSessionRecord = this.requireSession(this.activeId);
      const stored: OriginalDocumentStableIdentity = JSON.parse(session.identity) as OriginalDocumentStableIdentity;
      const core = document.core;
      return core.loaded === true && core.modelMayHaveChanged === false &&
        core.name === stored.name && core.format === stored.format &&
        core.byteCount === stored.byteCount && core.sha256 === stored.sha256 &&
        core.revision === stored.revision && core.source === stored.source;
    } catch (_) { return false; }
  }

  acceptClosed(document: OriginalDocumentEnvelope): void {
    this.requireIdle();
    let checked: OriginalDocumentEnvelope;
    try { checked = parseOriginalDocument(JSON.stringify(document)); }
    catch (error) { throw this.failure((error as Error).message); }
    const core: OriginalDocumentSessionOwnerCore = checked.core as OriginalDocumentSessionOwnerCore;
    if (!checked.passed || !core.passed || core.loaded || checked.sessionRetained !== false ||
      core.modelMayHaveChanged || core.contextRetained !== false || checked.error !== '' || core.error !== '') {
      throw this.failure('原生文档尚未确认关闭并释放会话及上下文');
    }
    this.invalidateNative();
  }

  remove(id: string): string {
    this.requireIdle();
    const session: OriginalDocumentSessionRecord = this.requireSession(id);
    if (id === this.nativeSessionId) throw this.failure('必须先实际关闭已绑定的原生文档');
    const index: number = this.sessions.indexOf(session);
    this.sessions.splice(index, 1);
    this.totalSourceBytes -= session.bytes.byteLength;
    let candidate: string = this.activeId;
    if (id === this.activeId) {
      this.activeId = '';
      candidate = this.sessions.length === 0 ? '' : this.sessions[Math.min(index, this.sessions.length - 1)].id;
    }
    this.changed();
    return candidate;
  }

  private requireIdle(): void {
    if (this.pending !== undefined) throw this.failure('文档切换尚未结束');
  }
  private requireSession(id: string): OriginalDocumentSessionRecord {
    for (const session of this.sessions) {
      if (session.id === id) return session;
    }
    throw this.failure('文档会话不存在');
  }
  private requireBound(id: string): OriginalDocumentSessionRecord {
    const session: OriginalDocumentSessionRecord = this.requireSession(id);
    if (id !== this.activeId || id !== this.nativeSessionId) throw this.failure('文档不是当前已绑定的原生会话');
    return session;
  }
  private requireTicket(request: OriginalDocumentActivation): OriginalDocumentPendingActivation {
    if (this.pending === undefined || request === null || typeof request !== 'object' ||
      request.id !== this.pending.id || request.token !== this.pending.token) throw this.failure('切换请求已过期或不匹配');
    return this.pending;
  }
  private validateSource(document: OriginalDocumentEnvelope, bytes: Uint8Array): OriginalDocumentEnvelope {
    try { return checkedSource(document, bytes); }
    catch (error) { throw this.failure((error as Error).message); }
  }
  private changed(): void { this.sequence++; this.error = ''; }
  private failure(message: string): Error { this.error = message; return new Error(message); }
}
