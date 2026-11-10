import { common } from '@kit.AbilityKit';
import { fileIo, picker } from '@kit.CoreFileKit';
import { MAXIMUM_OPTIONS_DOCUMENT_BYTES, OptionsDocumentPort, OptionsDocumentByteStream, readBoundedOptionsStream, writeAndVerifyOptionsStream } from './OptionsDocumentTransfer';

class HarmonyOptionsByteStream implements OptionsDocumentByteStream {
  private file: fileIo.File;
  constructor(file: fileIo.File) { this.file = file; }
  read(maximum: number): Uint8Array {
    const buffer: ArrayBuffer = new ArrayBuffer(maximum);
    const count: number = fileIo.readSync(this.file.fd, buffer);
    if (!Number.isInteger(count) || count < 0 || count > maximum) throw new Error('设置文件读取未通过');
    return new Uint8Array(buffer).slice(0, count);
  }
  write(bytes: Uint8Array): number {
    return fileIo.writeSync(this.file.fd, bytes.buffer, { length: bytes.byteLength });
  }
  seekStart(): void { fileIo.lseek(this.file.fd, 0, fileIo.WhenceType.SEEK_SET); }
  flush(): void { fileIo.fsyncSync(this.file.fd); }
}
export class HarmonyOptionsDocumentPort implements OptionsDocumentPort {
  private context: common.UIAbilityContext;
  constructor(context: common.UIAbilityContext) { this.context = context; }
  async selectExport(): Promise<string[]> {
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const options: picker.DocumentSaveOptions = { newFileNames: ['olex2-settings.options'],
      fileSuffixChoices: ['Olex2 settings|.options'] };
    return view.save(options);
  }
  async selectImport(): Promise<string[]> {
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const options: picker.DocumentSelectOptions = { maxSelectNumber: 1,
      fileSuffixFilters: ['Olex2 settings|.options', 'Text|.txt'] };
    return view.select(options);
  }
  read(uri: string): Uint8Array {
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
    try {
      if (fileIo.statSync(file.fd).size > MAXIMUM_OPTIONS_DOCUMENT_BYTES) throw new Error('设置文件不能超过 1 MiB');
      return readBoundedOptionsStream(new HarmonyOptionsByteStream(file));
    } finally { fileIo.closeSync(file); }
  }
  writeVerified(uri: string, data: Uint8Array): void {
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
    try { writeAndVerifyOptionsStream(new HarmonyOptionsByteStream(file), data);
    } finally { fileIo.closeSync(file); }
  }
}
