import nativeBridge, { ApplicationRuntimeStatus, PythonRuntimeStatus } from 'libolex2native.so';
import { MainWindowFontMetrics } from '../model/MainWindowFontMetrics';
import { flushOriginalDocumentRecoveryBeforePause } from '../model/OriginalDocumentRecoveryLifecycle';
import { UIAbility } from '@kit.AbilityKit';
import { bundleManager } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

export default class EntryAbility extends UIAbility {
  private fontMetrics: MainWindowFontMetrics = new MainWindowFontMetrics();
  onBackground(): void { this.flushRecoveryBeforePause(); }
  onWindowStageDestroy(): void { this.flushRecoveryBeforePause(); this.fontMetrics.stop(); }
  onDestroy(): void { this.flushRecoveryBeforePause(); this.fontMetrics.stop(); nativeBridge.shutdownApplicationRuntime(); }
  private flushRecoveryBeforePause(): void {
    try { flushOriginalDocumentRecoveryBeforePause(); }
    catch (error) { hilog.error(0x3801, 'Olex2Native', 'OLEX2_RECOVERY_LIFECYCLE_ERROR %{public}s', (error as Error).message); }
  }

  onWindowStageCreate(stage: window.WindowStage): void {
    AppStorage.setOrCreate<string>('olex2FilesDir', this.context.filesDir);
    // Read the installed bundle; never substitute a source/version placeholder.
    try {
      const info = bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT);
      AppStorage.setOrCreate<string>('olex2BundleName', info.name);
      AppStorage.setOrCreate<string>('olex2VersionName', info.versionName);
      AppStorage.setOrCreate<number>('olex2VersionCode', info.versionCode);
      AppStorage.setOrCreate<string>('olex2PackageIdentityError', '');
    } catch (error) {
      AppStorage.setOrCreate<string>('olex2BundleName', '');
      AppStorage.setOrCreate<string>('olex2VersionName', '');
      AppStorage.setOrCreate<number>('olex2VersionCode', 0);
      AppStorage.setOrCreate<string>('olex2PackageIdentityError', '无法读取已安装版本');
      hilog.error(0x3801, 'Olex2Native', 'OLEX2_PACKAGE_IDENTITY_ERROR %{public}s', JSON.stringify(error));
    }
    try {
      // Do not automatically rerun the historical platform diagnostic suite.
      // The original Python module registry must be prepared before any lazy FS use.
      const runtime: ApplicationRuntimeStatus = nativeBridge.initializeApplicationRuntime(this.context.filesDir,
        this.context.resourceManager.getRawFileContentSync('OLEX2-LICENCE.txt'));
      if (!runtime.initialized) hilog.error(0x3801, 'Olex2Native', 'OLEX2_RUNTIME_ERROR %{public}s', runtime.error);
      if (runtime.initialized) {
        const python: PythonRuntimeStatus = nativeBridge.initializePythonRuntime(this.context.resourceManager.getRawFileContentSync('python311.zip'));
        if (!python.initialized) hilog.error(0x3801, 'Olex2Native', 'OLEX2_PYTHON_ERROR %{public}s', python.error);
      }
    } catch (error) {
      hilog.error(0x3801, 'Olex2Native', 'OLEX2_RUNTIME_ERROR %{public}s', JSON.stringify(error));
    }
    this.fontMetrics.start(stage);
    stage.loadContent('pages/Index', (error) => {
      if (error.code) {
        hilog.error(0x3801, 'Olex2Native', 'OLEX2_PAGE_ERROR %{public}s', JSON.stringify(error));
      } else {
        hilog.info(0x3801, 'Olex2Native', 'OLEX2_PAGE_READY');
      }
    });
  }
}
