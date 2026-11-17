import { common } from '@kit.AbilityKit';
import { fileIo, picker } from '@kit.CoreFileKit';
import { MAXIMUM_PYTHON_DOCUMENT_BYTES, PythonDocumentPort, PythonDocumentByteStream, readPythonStream, writePythonStream } from './PythonDocument';

class HarmonyPythonByteStream implements PythonDocumentByteStream {
  private file: fileIo.File;
  constructor(file: fileIo.File) { this.file = file; }
  read(maximum: number): Uint8Array {
    const buffer: ArrayBuffer = new ArrayBuffer(maximum);
    const count: number = fileIo.readSync(this.file.fd, buffer);
    if (!Number.isInteger(count) || count < 0 || count > maximum) throw new Error('脚本文件读取未完成');
    return new Uint8Array(buffer).slice(0, count);
  }
  write(bytes: Uint8Array): number { return fileIo.writeSync(this.file.fd, bytes.buffer, { length: bytes.byteLength }); }
  seekStart(): void { fileIo.lseek(this.file.fd, 0, fileIo.WhenceType.SEEK_SET); }
  flush(): void { fileIo.fsyncSync(this.file.fd); }
}
export class HarmonyPythonDocumentPort implements PythonDocumentPort {
  private context: common.UIAbilityContext;
  constructor(context: common.UIAbilityContext) { this.context = context; }
  async selectImport(): Promise<string[]> {
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const options: picker.DocumentSelectOptions = { maxSelectNumber: 1,
      fileSuffixFilters: ['Python script|.py', 'Text|.txt'] };
    return view.select(options);
  }
  async selectExport(): Promise<string[]> {
    const view: picker.DocumentViewPicker = new picker.DocumentViewPicker(this.context);
    const options: picker.DocumentSaveOptions = { newFileNames: ['harmony-script.py'], fileSuffixChoices: ['Python script|.py'] };
    return view.save(options);
  }
  read(uri: string): Uint8Array {
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_ONLY);
    try {
      if (fileIo.statSync(file.fd).size > MAXIMUM_PYTHON_DOCUMENT_BYTES) throw new Error('脚本文件不能超过 64 KiB');
      return readPythonStream(new HarmonyPythonByteStream(file));
    } finally { fileIo.closeSync(file); }
  }
  writeVerified(uri: string, data: Uint8Array): void {
    const file: fileIo.File = fileIo.openSync(uri, fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
    try { writePythonStream(new HarmonyPythonByteStream(file), data);
    } finally { fileIo.closeSync(file); }
  }
}
