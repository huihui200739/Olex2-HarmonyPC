import nativeBridge from 'libolex2native.so';
import { UIAbility } from '@kit.AbilityKit';
import { bundleManager } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

export default class EntryAbility extends UIAbility {
  onDestroy(): void { nativeBridge.shutdownApplicationRuntime(); }

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
      const runtime = nativeBridge.initializeApplicationRuntime(this.context.filesDir,
        this.context.resourceManager.getRawFileContentSync('OLEX2-LICENCE.txt'));
      if (!runtime.initialized) hilog.error(0x3801, 'Olex2Native', 'OLEX2_RUNTIME_ERROR %{public}s', runtime.error);
    } catch (error) {
      hilog.error(0x3801, 'Olex2Native', 'OLEX2_RUNTIME_ERROR %{public}s', JSON.stringify(error));
    }
    stage.loadContent('pages/Index', (error) => {
      if (error.code) {
        hilog.error(0x3801, 'Olex2Native', 'OLEX2_PAGE_ERROR %{public}s', JSON.stringify(error));
      } else {
        hilog.info(0x3801, 'Olex2Native', 'OLEX2_PAGE_READY');
      }
    });
  }
}
