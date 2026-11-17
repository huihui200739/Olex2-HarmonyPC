// Bounded text documents only: loading never executes code. Trusted scripts run
// explicitly in the existing application VM, not in a security sandbox.
export const MAXIMUM_PYTHON_DOCUMENT_BYTES: number = 64 * 1024;

export interface PythonDocumentByteStream {
  read(maximum: number): Uint8Array;
  write(bytes: Uint8Array): number;
  seekStart(): void;
  flush(): void;
}
export interface PythonDocumentPort {
  selectImport(): Promise<string[]>;
  selectExport(): Promise<string[]>;
  read(uri: string): Uint8Array;
  writeVerified(uri: string, data: Uint8Array): void;
}
export interface PythonDocumentStatus {
  sequence: number; operation: string; state: string; selectedUri: string;
  byteCount: number; destinationMayBePartial: boolean; error: string;
}
export interface PythonDocumentResult { source: string; status: PythonDocumentStatus; }
export function emptyPythonDocumentStatus(): PythonDocumentStatus {
  return { sequence: 0, operation: '', state: 'idle', selectedUri: '', byteCount: 0,
    destinationMayBePartial: false, error: '' };
}
export function decodePythonDocument(data: Uint8Array): string {
  if (data.byteLength === 0 || data.byteLength > MAXIMUM_PYTHON_DOCUMENT_BYTES) {
    throw new Error('脚本文件必须为 1 字节至 64 KiB');
  }
  const text: string[] = [];
  let at: number = 0;
  while (at < data.byteLength) {
    const first: number = data[at++];
    let point: number = first;
    let remaining: number = 0;
    let minimum: number = 0;
    if (first >= 0xC2 && first <= 0xDF) { point = first & 31; remaining = 1; minimum = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { point = first & 15; remaining = 2; minimum = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { point = first & 7; remaining = 3; minimum = 0x10000; }
    else if (first >= 0x80) throw new Error('仅支持有效 UTF-8 编码的脚本');
    if (at + remaining > data.byteLength) throw new Error('脚本 UTF-8 字符不完整');
    for (let i: number = 0; i < remaining; i++) {
      const next: number = data[at++];
      if (next < 0x80 || next > 0xBF) throw new Error('仅支持有效 UTF-8 编码的脚本');
      point = point * 64 + (next & 63);
    }
    if (point < minimum || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) {
      throw new Error('仅支持有效 UTF-8 编码的脚本');
    }
    if (point === 0) throw new Error('脚本不能包含 NUL 字符');
    if (text.length === 0 && at === 3 && point === 0xFEFF) continue;
    if (point <= 0xFFFF) text.push(String.fromCharCode(point));
    else { point -= 0x10000; text.push(String.fromCharCode(0xD800 + (point >> 10), 0xDC00 + (point & 1023))); }
  }
  if (text.length === 0) throw new Error('脚本文件没有正文');
  return text.join('');
}
export function encodePythonDocument(source: string): Uint8Array {
  if (source.length === 0 || source.length > MAXIMUM_PYTHON_DOCUMENT_BYTES) throw new Error('脚本必须为 1 字节至 64 KiB');
  const bytes: number[] = [];
  for (let at: number = 0; at < source.length; at++) {
    let point: number = source.charCodeAt(at);
    if (point === 0) throw new Error('脚本不能包含 NUL 字符');
    if (point >= 0xD800 && point <= 0xDBFF) {
      const low: number = source.charCodeAt(++at);
      if (!Number.isInteger(low) || low < 0xDC00 || low > 0xDFFF) throw new Error('脚本包含不完整 Unicode 字符');
      point = 0x10000 + (point - 0xD800) * 1024 + low - 0xDC00;
    } else if (point >= 0xDC00 && point <= 0xDFFF) throw new Error('脚本包含不完整 Unicode 字符');
    if (point < 0x80) bytes.push(point);
    else if (point < 0x800) bytes.push(0xC0 + (point >> 6), 0x80 + (point & 63));
    else if (point < 0x10000) bytes.push(0xE0 + (point >> 12), 0x80 + ((point >> 6) & 63), 0x80 + (point & 63));
    else bytes.push(0xF0 + (point >> 18), 0x80 + ((point >> 12) & 63), 0x80 + ((point >> 6) & 63), 0x80 + (point & 63));
    if (bytes.length > MAXIMUM_PYTHON_DOCUMENT_BYTES) throw new Error('脚本 UTF-8 内容不能超过 64 KiB');
  }
  return new Uint8Array(bytes);
}
export function readPythonStream(stream: PythonDocumentByteStream): Uint8Array {
  const chunks: Uint8Array[] = [];
  let length: number = 0;
  while (true) {
    const maximum: number = Math.min(4096, MAXIMUM_PYTHON_DOCUMENT_BYTES + 1 - length);
    const chunk: Uint8Array = stream.read(maximum);
    if (chunk.byteLength > maximum || length + chunk.byteLength > MAXIMUM_PYTHON_DOCUMENT_BYTES) throw new Error('脚本文件不能超过 64 KiB');
    if (chunk.byteLength === 0) break;
    chunks.push(chunk.slice()); length += chunk.byteLength;
  }
  const bytes: Uint8Array = new Uint8Array(length);
  let offset: number = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
  return bytes;
}
export function writePythonStream(stream: PythonDocumentByteStream, bytes: Uint8Array): void {
  if (bytes.byteLength === 0 || bytes.byteLength > MAXIMUM_PYTHON_DOCUMENT_BYTES) throw new Error('脚本必须为 1 字节至 64 KiB');
  let offset: number = 0;
  while (offset < bytes.byteLength) {
    const count: number = stream.write(bytes.slice(offset));
    if (!Number.isInteger(count) || count <= 0 || count > bytes.byteLength - offset) throw new Error('脚本文件写入未完成');
    offset += count;
  }
  stream.flush(); stream.seekStart();
  const actual: Uint8Array = readPythonStream(stream);
  if (actual.byteLength !== bytes.byteLength) throw new Error('保存后脚本文件长度不一致');
  for (let i: number = 0; i < actual.byteLength; i++) {
    if (actual[i] !== bytes[i]) throw new Error('保存后脚本文件内容不一致');
  }
}
export class PythonDocumentTransfer {
  private active: boolean = false;
  private sequence: number = 0;
  async run(operation: string, source: string, port: PythonDocumentPort): Promise<PythonDocumentResult> {
    if (this.active) throw new Error('正在处理另一个脚本文件');
    this.active = true;
    const status: PythonDocumentStatus = { sequence: ++this.sequence, operation: operation, state: 'choosing',
      selectedUri: '', byteCount: 0, destinationMayBePartial: false, error: '' };
    let text: string = source;
    try {
      if (operation !== 'import' && operation !== 'export') throw new Error('不支持的脚本文件操作');
      const bytes: Uint8Array = operation === 'export' ? encodePythonDocument(source) : new Uint8Array(0);
      const uris: string[] = operation === 'import' ? await port.selectImport() : await port.selectExport();
      if (uris.length === 0) { status.state = 'cancelled'; return { source: source, status: status }; }
      if (uris.length !== 1 || !uris[0].startsWith('file://')) throw new Error('请选择一个系统文档文件');
      status.selectedUri = uris[0];
      if (operation === 'import') {
        const input: Uint8Array = port.read(uris[0]);
        text = decodePythonDocument(input); status.byteCount = input.byteLength;
      } else {
        status.destinationMayBePartial = true;
        port.writeVerified(uris[0], bytes); status.byteCount = bytes.byteLength;
        status.destinationMayBePartial = false;
      }
      status.state = 'passed';
    } catch (error) {
      status.state = 'failed'; status.error = (error as Error).message || '脚本文件操作未通过';
    } finally { this.active = false; }
    return { source: text, status: status };
  }
}
