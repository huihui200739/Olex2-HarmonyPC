import { common } from '@kit.AbilityKit';
import { BusinessError, deviceInfo } from '@kit.BasicServicesKit';
import { picker } from '@kit.CoreFileKit';
import { window } from '@kit.ArkUI';
import { MainFrameFileRequest, mainFramePickerPlan, mainFrameSelectedUri } from './MainFrameFilePicker';

export interface MainFrameRectPx { left: number; top: number; width: number; height: number; }
export type MainFrameDone = (error: string, cancelled: boolean, uri: string) => void;

// This object is created with the actual UIAbility window. It does not create,
// move or close windows, start a picker, or execute application commands at init.
export class HarmonyMainFramePlatform {
  private target: window.Window;
  private documents: picker.DocumentViewPicker;
  constructor(target: window.Window, context: common.UIAbilityContext) {
    this.target = target;
    this.documents = new picker.DocumentViewPicker(context);
  }
  readWindowRect(): MainFrameRectPx {
    const rect: window.Rect = this.target.getWindowProperties().windowRect;
    return { left: rect.left, top: rect.top, width: rect.width, height: rect.height };
  }
  private error(operation: string, value: BusinessError): string {
    // Do not forward provider URIs, paths or document contents through errors.
    return `${operation} failed (${value.code})`;
  }
  moveWindow(left: number, top: number, done: MainFrameDone): void {
    try {
      this.target.moveWindowTo(left, top).then(() => done('', false, ''))
        .catch((error: BusinessError) => done(this.error('Moving the window', error), false, ''));
    } catch (error) { done('Moving the window could not be requested', false, ''); }
  }
  resizeWindow(width: number, height: number, done: MainFrameDone): void {
    try {
      this.target.resize(width, height).then(() => done('', false, ''))
        .catch((error: BusinessError) => done(this.error('Resizing the window', error), false, ''));
    } catch (error) { done('Resizing the window could not be requested', false, ''); }
  }
  setWindowTitle(title: string, done: MainFrameDone): void {
    // API15 title changes are optional; retain the API12 project minimum.
    if (deviceInfo.sdkApiVersion < 15 || !canIUse('SystemCapability.Window.SessionManager') || typeof this.target.setWindowTitle !== 'function') {
      done('This device does not provide the API15 system-window title capability', false, ''); return;
    }
    try {
      this.target.setWindowTitle(title).then(() => done('', false, ''))
        .catch((error: BusinessError) => done(this.error('Changing the window title', error), false, ''));
    } catch (error) { done('Changing the window title could not be requested', false, ''); }
  }
  pickFile(request: MainFrameFileRequest, done: MainFrameDone): void {
    try {
      const plan = mainFramePickerPlan(request);
      let selection: Promise<string[]>;
      if (request.open) {
        const options: picker.DocumentSelectOptions = { maxSelectNumber: 1 };
        if (plan.filters.length > 0) options.fileSuffixFilters = plan.filters;
        if (plan.defaultFolderUri.length > 0) options.defaultFilePathUri = plan.defaultFolderUri;
        selection = this.documents.select(options);
      } else {
        const options: picker.DocumentSaveOptions = {};
        if (plan.filters.length > 0) options.fileSuffixChoices = plan.filters;
        if (plan.defaultFolderUri.length > 0) options.defaultFilePathUri = plan.defaultFolderUri;
        if (plan.defaultFileName.length > 0) options.newFileNames = [plan.defaultFileName];
        selection = this.documents.save(options);
      }
      // System Picker has no custom-caption option. The requested caption is
      // retained in the request but is not misreported as a changed system title.
      selection.then((uris: string[]) => {
        try { const uri: string = mainFrameSelectedUri(uris); done('', uri.length === 0, uri); }
        catch (error) { done('The system picker returned an invalid document selection', false, ''); }
      }).catch((error: BusinessError) => done(this.error('Document selection', error), false, ''));
    } catch (error) { done('Document selection could not be requested with these options', false, ''); }
  }
}
export async function createHarmonyMainFramePlatform(context: common.UIAbilityContext): Promise<HarmonyMainFramePlatform> {
  const target: window.Window = await window.getLastWindow(context);
  return new HarmonyMainFramePlatform(target, context);
}
