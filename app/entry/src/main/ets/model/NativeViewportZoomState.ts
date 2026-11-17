// Ordinary view-control units only. No document/domain command is executed here.
export function nativeViewportZoomTenths(distance: number): number {
  const safe = Number.isFinite(distance) ? distance : 4.4;
  return Math.max(22, Math.min(100, Math.round(safe * 10)));
}
export function nativeViewportZoomEvent(value: number, enabled: boolean, disposed: boolean): number | undefined {
  if (!enabled || disposed || !Number.isFinite(value)) return undefined;
  return nativeViewportZoomTenths(value) / 10;
}
