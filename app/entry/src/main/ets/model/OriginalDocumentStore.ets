import { common } from '@kit.AbilityKit';
import { fileIo, picker } from '@kit.CoreFileKit';
import { MAXIMUM_ORIGINAL_DOCUMENT_BYTES, OriginalDocumentPort, OriginalDocumentByteStream,
  originalDocumentFormat, validateOriginalDocumentName, validateOriginalDocumentBytes, readOriginalDocumentStream,
  writeOriginalDocumentStream } from './OriginalDocumentTransfer';

class HarmonyOriginalDocumentByteStream implements OriginalDocumentByteStream {
  private file: fileIo.File;
  constructor(file: fileIo.File) { this.file = file; }
  read(maximum: number): Uint8Array {
    const buffer: ArrayBuffer = new ArrayBuffer(maximum);
    const count: number = fileIo.readSync(this.file.fd, buffer);
    if (!Number.isInteger(count) || count < 0 || count > maximum) throw new Error('原版文档读取未完成');
    return new Uint8Array(buffer).slice(0, count);
  }
  write(bytes: Uint8Array): number {
    // Copy a sliced view so a provider never receives unrelated backing bytes.
    const data: Uint8Array = bytes.slice();
    return fileIo.writeSync(this.file.fd, data.buffer, { length: data.byteLength });
  }
  seekStart(): void { fileIo.lseek(this.file.fd, 0, fileIo.WhenceType.SEEK_SET); }
  flush(): void { fileIo.fsyncSync(this.file.fd); }
}
export class HarmonyOriginalDocumentPort implements OriginalDocumentPort {
  private context: common.UIAbilityContext;
  constructor(context: common.UIAbilityContext) { this.context = context; }
  async selectImport(): Promise<string[]> {
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const options: picker.DocumentSelectOptions = { maxSelectNumber: 1,
      fileSuffixFilters: ['CIF document|.cif', 'INS document|.ins', 'RES document|.res'] };
    return view.select(options);
  }
  async selectExport(name: string): Promise<string[]> {
    validateOriginalDocumentName(name);
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const format: string = originalDocumentFormat(name);
    const options: picker.DocumentSaveOptions = { newFileNames: [name],
      fileSuffixChoices: [`Original ${format.toUpperCase()}|.${format}`] };
    return view.save(options);
  }
  read(uri: string): Uint8Array {
    // The provider URI is passed intact to fileIo. Never derive a local path.
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
    try {
      if (fileIo.statSync(file.fd).size > MAXIMUM_ORIGINAL_DOCUMENT_BYTES) throw new Error('原版文档不能超过 8 MiB');
      return readOriginalDocumentStream(new HarmonyOriginalDocumentByteStream(file));
    } finally { fileIo.closeSync(file); }
  }
  writeVerified(uri: string, data: Uint8Array): void {
    validateOriginalDocumentBytes(data);
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
    try { writeOriginalDocumentStream(new HarmonyOriginalDocumentByteStream(file), data); }
    finally { fileIo.closeSync(file); }
  }
}
