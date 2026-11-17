// Pure platform-session metadata. No I/O, native ownership, revisions or commands.
// Digest strings are format-checked only; they are NOT proof of actual native/source provenance.
import { MAXIMUM_ORIGINAL_DOCUMENT_BYTES, validateOriginalDocumentName } from './OriginalDocumentTransfer';

export interface OriginalDocumentViewportState {
  schemaVersion: 1;
  name: string;
  byteCount: number;
  sha256: string;
  yaw: number;
  pitch: number;
  distance: number;
  objectCount: number;
  inventorySha256: string;
  selectedObjectIndices: number[];
}

const MAXIMUM_VIEWPORT_JSON_BYTES: number = 1024 * 1024;
const MAXIMUM_SELECTED_OBJECTS: number = 65536;
const CAMERA_ABSOLUTE_TOLERANCE: number = 1e-4;

// Bound the original JSON BEFORE parsing, including whitespace and UTF-8 multibyte names.
function requireBoundedViewportJson(text: string): void {
  if (typeof text !== 'string' || text.length === 0 || text.length > MAXIMUM_VIEWPORT_JSON_BYTES) {
    throw new Error('视口 JSON 必须为不超过 1 MiB UTF-8 的文本');
  }
  let length: number = 0;
  for (let i: number = 0; i < text.length; i++) {
    const code: number = text.charCodeAt(i);
    if (code < 0x80) length++;
    else if (code < 0x800) length += 2;
    else if (code >= 0xD800 && code <= 0xDBFF) {
      const low: number = text.charCodeAt(++i);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) throw new Error('视口 JSON 包含不完整 Unicode');
      length += 4;
    } else if (code >= 0xDC00 && code <= 0xDFFF) throw new Error('视口 JSON 包含不完整 Unicode');
    else length += 3;
    if (length > MAXIMUM_VIEWPORT_JSON_BYTES) throw new Error('视口 JSON 超出 1 MiB UTF-8 上限');
  }
}

function requireViewportShape(state: OriginalDocumentViewportState): void {
  if (state === null || typeof state !== 'object' || Array.isArray(state)) throw new Error('视口必须为对象');
  const expected: string[] = ['schemaVersion', 'name', 'byteCount', 'sha256', 'yaw', 'pitch', 'distance',
    'objectCount', 'inventorySha256', 'selectedObjectIndices'];
  const fields: string[] = Object.keys(state);
  if (fields.length !== expected.length) throw new Error('视口字段不完整或包含未知字段；不接受 native 所有权或 revision');
  for (let i: number = 0; i < expected.length; i++) {
    if (fields.indexOf(expected[i]) < 0) throw new Error('视口缺少必需字段');
  }
}

function requireDigest(value: string): void {
  if (typeof value !== 'string' || !/^[0-9a-f]{64}$/.test(value)) throw new Error('视口摘要必须为 64 位小写十六进制');
}

function requireCamera(value: number, minimum: number, maximum: number): void {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < minimum || value > maximum) {
    throw new Error('视口相机参数必须为范围内的有限数值');
  }
}

export function copyOriginalDocumentViewport(state: OriginalDocumentViewportState): OriginalDocumentViewportState {
  requireViewportShape(state);
  if (state.schemaVersion !== 1 || typeof state.name !== 'string' ||
    !Number.isSafeInteger(state.byteCount) || state.byteCount < 1 || state.byteCount > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) {
    throw new Error('视口版本、文件名或来源容量不合法');
  }
  validateOriginalDocumentName(state.name);
  requireDigest(state.sha256); requireDigest(state.inventorySha256);
  requireCamera(state.yaw, -360, 360); requireCamera(state.pitch, -85, 85); requireCamera(state.distance, 2.2, 10);
  if (!Number.isSafeInteger(state.objectCount) || state.objectCount < 0 || state.objectCount > 1000000 ||
    !Array.isArray(state.selectedObjectIndices) || state.selectedObjectIndices.length > MAXIMUM_SELECTED_OBJECTS ||
    Object.keys(state.selectedObjectIndices).length !== state.selectedObjectIndices.length) {
    throw new Error('视口对象数量或选择列表不合法');
  }
  const selected: number[] = [];
  let previous: number = -1;
  for (let i: number = 0; i < state.selectedObjectIndices.length; i++) {
    const index: number = state.selectedObjectIndices[i];
    if (!Number.isSafeInteger(index) || index <= previous || index < 0 || index >= state.objectCount) {
      throw new Error('视口选择索引必须为范围内严格升序且唯一的整数');
    }
    selected.push(index); previous = index;
  }
  const copy: OriginalDocumentViewportState = { schemaVersion: 1, name: state.name, byteCount: state.byteCount,
    sha256: state.sha256, yaw: state.yaw, pitch: state.pitch, distance: state.distance,
    objectCount: state.objectCount, inventorySha256: state.inventorySha256, selectedObjectIndices: selected };
  requireBoundedViewportJson(JSON.stringify(copy));
  return copy;
}

export function parseOriginalDocumentViewport(raw: string): OriginalDocumentViewportState {
  requireBoundedViewportJson(raw);
  return copyOriginalDocumentViewport(JSON.parse(raw) as OriginalDocumentViewportState);
}

export function originalDocumentViewportMatchesSource(state: OriginalDocumentViewportState,
  name: string, byteCount: number, sha256: string): boolean {
  try {
    const checked: OriginalDocumentViewportState = copyOriginalDocumentViewport(state);
    return checked.name === name && checked.byteCount === byteCount && checked.sha256 === sha256;
  } catch (_) { return false; }
}

export function equalOriginalDocumentViewport(expected: OriginalDocumentViewportState,
  actual: OriginalDocumentViewportState): boolean {
  try {
    const left: OriginalDocumentViewportState = copyOriginalDocumentViewport(expected);
    const right: OriginalDocumentViewportState = copyOriginalDocumentViewport(actual);
    if (left.schemaVersion !== right.schemaVersion || left.name !== right.name || left.byteCount !== right.byteCount ||
      left.sha256 !== right.sha256 || left.objectCount !== right.objectCount || left.inventorySha256 !== right.inventorySha256 ||
      Math.abs(left.yaw - right.yaw) > CAMERA_ABSOLUTE_TOLERANCE ||
      Math.abs(left.pitch - right.pitch) > CAMERA_ABSOLUTE_TOLERANCE ||
      Math.abs(left.distance - right.distance) > CAMERA_ABSOLUTE_TOLERANCE ||
      left.selectedObjectIndices.length !== right.selectedObjectIndices.length) return false;
    for (let i: number = 0; i < left.selectedObjectIndices.length; i++) {
      if (left.selectedObjectIndices[i] !== right.selectedObjectIndices[i]) return false;
    }
    return true;
  } catch (_) { return false; }
}
