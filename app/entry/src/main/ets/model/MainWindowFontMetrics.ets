import nativeBridge, { FontDisplayMetricsStatus } from 'libolex2native.so';
import { display, window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { FontDisplayObservation, validFontDisplayObservation } from './FontDisplayMetrics';

// Reads this ability's main window, not an assumed default monitor. Every
// display event re-queries the window's current display to avoid stale events.
export class MainWindowFontMetrics {
  private main: window.Window | undefined = undefined;
  private displayListener: boolean = false;
  private windowListener: boolean = false;
  private active: boolean = false;
  private displayChanged = (_id: number): void => { this.refresh(); };
  private windowChanged = (_id: number): void => { this.refresh(); };

  start(stage: window.WindowStage): void {
    this.stop();
    this.active = true;
    try {
      this.main = stage.getMainWindowSync();
      this.main.on('displayIdChange', this.windowChanged);
      this.windowListener = true;
      display.on('change', this.displayChanged);
      this.displayListener = true;
      this.refresh();
    } catch (error) {
      this.fail('无法订阅当前窗口的屏幕参数：' + JSON.stringify(error));
      this.stop();
    }
  }

  private fail(message: string): void {
    nativeBridge.clearFontDisplayMetrics();
    AppStorage.setOrCreate<boolean>('olex2FontMetricsReady', false);
    AppStorage.setOrCreate<string>('olex2FontMetricsError', message);
    hilog.error(0x3801, 'Olex2Native', 'OLEX2_FONT_DISPLAY_ERROR %{public}s', message);
  }

  private refresh(): void {
    if (!this.active || this.main === undefined) return;
    try {
      const id = this.main.getWindowProperties().displayId;
      if (id === undefined) throw new Error('当前窗口没有报告displayId');
      const current = display.getDisplayByIdSync(id);
      const observation: FontDisplayObservation = { displayId: id, dpiX: current.xDPI,
        dpiY: current.yDPI, densityPixels: current.densityPixels };
      if (current.id !== id || !validFontDisplayObservation(observation)) {
        throw new Error('实际显示器身份或DPI无效，未使用默认屏幕或固定96替代');
      }
      const result: FontDisplayMetricsStatus = nativeBridge.setFontDisplayMetrics(observation.displayId, observation.dpiX,
        observation.dpiY, observation.densityPixels);
      if (!result.ready || result.displayId !== id || result.dpiX !== current.xDPI ||
        result.dpiY !== current.yDPI || result.densityPixels !== current.densityPixels) {
        throw new Error('原生屏幕参数回读与实际显示器不一致');
      }
      AppStorage.setOrCreate<boolean>('olex2FontMetricsReady', true);
      AppStorage.setOrCreate<string>('olex2FontMetricsError', '');
      hilog.info(0x3801, 'Olex2Native', 'OLEX2_FONT_DISPLAY_READY %{public}s', JSON.stringify(result));
    } catch (error) {
      this.fail('当前窗口的字体DPI不可用：' + JSON.stringify(error));
    }
  }

  stop(): void {
    this.active = false;
    if (this.displayListener) {
      try { display.off('change', this.displayChanged); } catch (error) {
        hilog.error(0x3801, 'Olex2Native', 'OLEX2_FONT_DISPLAY_UNSUBSCRIBE_ERROR %{public}s', JSON.stringify(error));
      }
    }
    if (this.windowListener && this.main !== undefined) {
      try { this.main.off('displayIdChange', this.windowChanged); } catch (error) {
        hilog.error(0x3801, 'Olex2Native', 'OLEX2_FONT_WINDOW_UNSUBSCRIBE_ERROR %{public}s', JSON.stringify(error));
      }
    }
    this.displayListener = false;
    this.windowListener = false;
    this.main = undefined;
    nativeBridge.clearFontDisplayMetrics();
    AppStorage.setOrCreate<boolean>('olex2FontMetricsReady', false);
  }
}
