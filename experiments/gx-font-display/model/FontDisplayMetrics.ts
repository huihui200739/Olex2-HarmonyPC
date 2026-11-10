// Pure validation only. Callers must obtain observations from the actual main window.
export interface FontDisplayObservation {
  displayId: number;
  dpiX: number;
  dpiY: number;
  densityPixels: number;
}
export function validFontDisplayObservation(value: FontDisplayObservation): boolean {
  return Number.isSafeInteger(value.displayId) && value.displayId >= 0 &&
    Number.isFinite(value.dpiX) && value.dpiX > 0 && value.dpiX <= 8192 &&
    Number.isFinite(value.dpiY) && value.dpiY > 0 && value.dpiY <= 8192 &&
    Number.isFinite(value.densityPixels) && value.densityPixels > 0 && value.densityPixels <= 64 &&
    value.dpiX / value.dpiY >= 0.0625 && value.dpiX / value.dpiY <= 16;
}
