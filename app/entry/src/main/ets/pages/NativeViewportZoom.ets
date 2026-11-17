import { NodeContent } from '@kit.ArkUI';
import nativeBridge from 'libolex2native.so';
import { nativeViewportZoomTenths, nativeViewportZoomEvent } from '../model/NativeViewportZoomState';

// The existing zoom operation now hosts original C++ TLabel/TTrackBar nodes.
// Native mount failure preserves the previous ArkTS slider instead of silently
// disabling zoom. Source/link qualification is not device-input acceptance.
@Component
export struct NativeViewportZoom {
  @Prop @Watch('syncNative') distance: number = 4.4;
  @Prop @Watch('syncNative') inputAllowed: boolean = true;
  onChange: (value: number) => void = (_value: number): void => {};
  private content: NodeContent = new NodeContent();
  private owner: number = 0;
  private disposed: boolean = false;
  @State private compatibilityFallback: boolean = false;

  aboutToAppear(): void { this.disposed = false; }
  aboutToDisappear(): void { this.disposed = true; this.detachNative(); }

  private changed(value: number): void {
    const next = nativeViewportZoomEvent(value, this.inputAllowed, this.disposed);
    if (next !== undefined) this.onChange(next);
  }

  private detachNative(): boolean {
    if (this.owner === 0) return true;
    try {
      nativeBridge.unmountNativeViewportZoom(this.owner);
      this.owner = 0;
      return true;
    } catch (error) {
      console.error('Original native zoom detach retained its owner: ' + `${error}`);
      return false;
    }
  }

  private failNative(): void {
    if (this.detachNative() && !this.disposed) this.compatibilityFallback = true;
  }

  private mountNative(): void {
    if (this.disposed || this.owner !== 0 || this.compatibilityFallback) return;
    try {
      this.owner = nativeBridge.mountNativeViewportZoom(this.content,
        nativeViewportZoomTenths(this.distance), this.inputAllowed,
        (value: number): void => this.changed(value));
    } catch (error) {
      console.error('Original native zoom mount failed: ' + `${error}`);
      this.failNative();
    }
  }

  private syncNative(): void {
    if (this.disposed || this.owner === 0) return;
    try {
      nativeBridge.updateNativeViewportZoom(this.owner,
        nativeViewportZoomTenths(this.distance), this.inputAllowed);
    } catch (error) {
      console.error('Original native zoom update failed: ' + `${error}`);
      this.failNative();
    }
  }

  build() {
    Row({ space: 8 }) {
      if (this.compatibilityFallback) {
        Text('缩放').fontSize(11).fontColor('#89958E')
        Slider({ value: this.distance, min: 2.2, max: 10, step: 0.1 })
          .id('olex2-view-zoom').enabled(this.inputAllowed).width(105)
          .selectedColor('#438563').trackColor('#E4ECE6')
          .onChange((value: number): void => this.changed(value))
      } else {
        ContentSlot(this.content)
      }
    }.width(145).height(32).id('olex2-native-zoom-host')
      .onAppear((): void => this.mountNative())
  }
}
