// Pure operation control. The platform adapter owns all picker and URI I/O.
export const MAXIMUM_OPTIONS_DOCUMENT_BYTES: number = 1024 * 1024;
export interface OptionsDocumentSnapshot {
  passed: boolean; data: Uint8Array; sha256: string; error: string;
}
export interface OptionsDocumentMerge {
  passed: boolean; optionsChanged: boolean; importedOptions: number; sha256: string; error: string;
}
export interface OptionsDocumentBackend {
  snapshot(): OptionsDocumentSnapshot;
  merge(data: Uint8Array): OptionsDocumentMerge;
}
export interface OptionsDocumentPort {
  selectExport(): Promise<string[]>;
  selectImport(): Promise<string[]>;
  read(uri: string): Uint8Array;
  writeVerified(uri: string, data: Uint8Array): void;
}
export interface OptionsDocumentStatus {
  sequence: number; operation: string; state: string; selectedUri: string;
  byteCount: number; sha256: string; importedOptions: number; optionsChanged: boolean;
  destinationMayBePartial: boolean; error: string;
}
export function emptyOptionsDocumentStatus(): OptionsDocumentStatus {
  return { sequence: 0, operation: '', state: 'idle', selectedUri: '', byteCount: 0, sha256: '',
    importedOptions: 0, optionsChanged: false, destinationMayBePartial: false, error: '' };
}
export class OptionsDocumentTransfer {
  private active: boolean = false;
  private sequence: number = 0;
  private current: OptionsDocumentStatus = emptyOptionsDocumentStatus();
  async run(operation: string, backend: OptionsDocumentBackend, port: OptionsDocumentPort): Promise<OptionsDocumentStatus> {
    if (this.active) return this.current;
    this.active = true;
    const status: OptionsDocumentStatus = { sequence: ++this.sequence, operation: operation, state: 'choosing',
      selectedUri: '', byteCount: 0, sha256: '', importedOptions: 0, optionsChanged: false,
      destinationMayBePartial: false, error: '' };
    this.current = status;
    let writeStarted: boolean = false;
    try {
      if (operation !== 'export' && operation !== 'import') throw new Error('Unsupported settings document operation');
      let snapshot: OptionsDocumentSnapshot = { passed: false, data: new Uint8Array(0), sha256: '', error: '' };
      if (operation === 'export') {
        snapshot = backend.snapshot();
        if (!snapshot.passed) throw new Error(snapshot.error || 'Cannot create original settings snapshot');
        if (snapshot.data.byteLength === 0 || snapshot.data.byteLength > MAXIMUM_OPTIONS_DOCUMENT_BYTES) throw new Error('Settings document must be 1 byte to 1 MiB');
      }
      const uris: string[] = operation === 'export' ? await port.selectExport() : await port.selectImport();
      if (uris.length === 0) { status.state = 'cancelled'; return status; }
      if (uris.length !== 1 || !uris[0].startsWith('file://')) throw new Error('Select exactly one system document URI');
      status.selectedUri = uris[0];
      if (operation === 'export') {
        // Snapshot at operation start, not a promise that concurrent later edits
        // are included. URI providers do not offer our private atomic rename.
        writeStarted = true;
        port.writeVerified(uris[0], snapshot.data);
        status.byteCount = snapshot.data.byteLength; status.sha256 = snapshot.sha256;
      } else {
        const data: Uint8Array = port.read(uris[0]);
        if (data.byteLength === 0 || data.byteLength > MAXIMUM_OPTIONS_DOCUMENT_BYTES) throw new Error('Settings document must be 1 byte to 1 MiB');
        const merged: OptionsDocumentMerge = backend.merge(data);
        if (!merged.passed) throw new Error(merged.error || 'Original settings document was not accepted');
        status.byteCount = data.byteLength; status.sha256 = merged.sha256;
        status.importedOptions = merged.importedOptions; status.optionsChanged = merged.optionsChanged;
      }
      status.state = 'passed';
    } catch (error) {
      status.state = 'failed'; status.destinationMayBePartial = writeStarted;
      status.error = (error as Error).message || 'Settings document operation failed';
    } finally {
      this.active = false;
    }
    return status;
  }
}

export interface OptionsDocumentByteStream {
  read(maximum: number): Uint8Array;
  write(bytes: Uint8Array): number;
  seekStart(): void;
  flush(): void;
}
export function readBoundedOptionsStream(stream: OptionsDocumentByteStream): Uint8Array {
  const parts: Uint8Array[] = [];
  let length: number = 0;
  while (true) {
    const maximum: number = Math.min(4096, MAXIMUM_OPTIONS_DOCUMENT_BYTES + 1 - length);
    const part: Uint8Array = stream.read(maximum);
    if (part.byteLength === 0) break;
    if (part.byteLength > maximum || length + part.byteLength > MAXIMUM_OPTIONS_DOCUMENT_BYTES) throw new Error('Settings document exceeds the 1 MiB limit');
    parts.push(part.slice()); length += part.byteLength;
  }
  const output: Uint8Array = new Uint8Array(length);
  let offset: number = 0;
  for (let i: number = 0; i < parts.length; i++) { output.set(parts[i], offset); offset += parts[i].byteLength; }
  return output;
}
export function writeAndVerifyOptionsStream(stream: OptionsDocumentByteStream, data: Uint8Array): void {
  if (data.byteLength === 0 || data.byteLength > MAXIMUM_OPTIONS_DOCUMENT_BYTES) throw new Error('Settings document must be 1 byte to 1 MiB');
  let written: number = 0;
  while (written < data.byteLength) {
    const count: number = stream.write(data.slice(written));
    if (!Number.isInteger(count) || count <= 0 || count > data.byteLength - written) throw new Error('Settings document write did not complete');
    written += count;
  }
  stream.flush(); stream.seekStart();
  const actual: Uint8Array = readBoundedOptionsStream(stream);
  if (actual.byteLength !== data.byteLength) throw new Error('Saved settings document length differs');
  for (let i: number = 0; i < actual.byteLength; i++) {
    if (actual[i] !== data[i]) throw new Error('Saved settings document bytes differ');
  }
}
