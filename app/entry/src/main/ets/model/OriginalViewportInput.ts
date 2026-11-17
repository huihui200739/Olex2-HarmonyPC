// Pure platform input model. No native calls, document edits or persistent settings.
// Every coordinate and size passed here is component-local ArkUI layout vp.
// Never divide these positions by native GraphicsStatus.width/height (pixels).
export const ORIGINAL_VIEWPORT_DRAG_THRESHOLD_VP: number = 6;
export interface OriginalViewportPoint { x: number; y: number; }
export interface OriginalViewportDrag { offsetX: number; offsetY: number; }
export interface OriginalSelectionRequest { action: string; x: number; y: number; mode: number; }
export interface OriginalViewportKeyResult { handled: boolean; action: string; }

export function originalSelectionRequest(action: string, x: number, y: number, mode: number): OriginalSelectionRequest {
  if ((action !== 'pick' && action !== 'clear-selection' && action !== 'select-all' && action !== 'invert-selection' && action !== 'toggle-cell' && action !== 'toggle-basis' && action !== 'toggle-frame' && action !== 'show-all' && action !== 'center-model' && action !== 'view-along-100' && action !== 'view-along-010' &&
      action !== 'view-along-001' && action !== 'view-along-110' && action !== 'view-along-101' &&
      action !== 'view-along-011' && action !== 'view-along-111' && action !== 'view-along-selection' && action !== 'hide-selection' && action !== 'toggle-grid' && action !== 'quality-1' && action !== 'quality-2' && action !== 'quality-3' && action !== 'toggle-hydrogens' &&
      action !== 'toggle-hydrogen-bonds' && action !== 'toggle-qpeaks' && action !== 'toggle-qpeak-bonds' && action !== 'toggle-labels' && action !== 'toggle-structure' &&
      action !== 'fragment-select' && action !== 'fragment-select-atoms' && action !== 'fragment-select-bonds' &&
      action !== 'fragment-hide' && action !== 'fragment-show-only') ||
    !Number.isFinite(x) || !Number.isFinite(y) || x < 0 || x > 1 || y < 0 || y > 1 ||
    !Number.isInteger(mode) || mode < 0 || mode > 2) {
    throw new Error('选择动作、视口坐标或选择模式不合法');
  }
  // All actions carry all four arguments, including non-positional commands.
  return { action: action, x: x, y: y, mode: mode };
}
export function normalizeOriginalViewportPoint(xVp: number, yVp: number,
  widthVp: number, heightVp: number): OriginalViewportPoint | undefined {
  if (!Number.isFinite(xVp) || !Number.isFinite(yVp) || !Number.isFinite(widthVp) || !Number.isFinite(heightVp) ||
    widthVp <= 0 || heightVp <= 0 || xVp < 0 || yVp < 0 || xVp > widthVp || yVp > heightVp) return undefined;
  // Top-left stays top-left; pixel conversion belongs exclusively to native.
  return { x: xVp / widthVp, y: yVp / heightVp };
}
export function originalVisibilityStateChanged(previous: boolean | undefined, actual: boolean | undefined): boolean {
  return typeof previous === 'boolean' && typeof actual === 'boolean' && previous !== actual;
}
export function originalSelectionMode(baseMode: number, ctrl: boolean, shift: boolean): number {
  if (!Number.isInteger(baseMode) || baseMode < 0 || baseMode > 2) throw new Error('选择模式不合法');
  // Desktop modifiers override only this pointer operation, not the toolbar.
  return ctrl ? 2 : shift ? 1 : baseMode;
}

export class OriginalViewportInput {
  private widthVp: number = 0;
  private heightVp: number = 0;
  private available: boolean = false;
  private busy: boolean = true;
  private focused: boolean = false;
  private pointerKind: string = '';
  private pointerId: number = -1;
  private startX: number = 0;
  private startY: number = 0;
  private dragged: boolean = false;
  private pressedKeys: Set<string> = new Set<string>();

  setViewportSizeVp(widthVp: number, heightVp: number): void {
    if (widthVp !== this.widthVp || heightVp !== this.heightVp) this.cancelPointer();
    this.widthVp = Number.isFinite(widthVp) && widthVp > 0 ? widthVp : 0;
    this.heightVp = Number.isFinite(heightVp) && heightVp > 0 ? heightVp : 0;
  }
  setContext(available: boolean, busy: boolean): void {
    this.available = available; this.busy = busy;
    if (!available || busy) this.cancelPointer();
  }
  setFocused(focused: boolean): void {
    this.focused = focused;
    if (!focused) { this.cancelPointer(); this.pressedKeys.clear(); }
  }
  cancelPointer(): void { this.pointerKind = ''; this.pointerId = -1; this.dragged = false; }
  markDragged(): void { if (this.pointerKind.length > 0) this.dragged = true; }
  pointerDown(kind: string, id: number, xVp: number, yVp: number, primary: boolean): boolean {
    if (!this.available || this.busy || !primary || (kind !== 'mouse' && kind !== 'touch') ||
      !Number.isSafeInteger(id) || id < 0 || this.pointerKind.length > 0 ||
      normalizeOriginalViewportPoint(xVp, yVp, this.widthVp, this.heightVp) === undefined) {
      this.cancelPointer(); return false;
    }
    this.pointerKind = kind; this.pointerId = id; this.startX = xVp; this.startY = yVp; this.dragged = false;
    return true;
  }
  pointerMove(kind: string, id: number, xVp: number, yVp: number): OriginalViewportDrag | undefined {
    if (this.pointerKind.length === 0 || kind !== this.pointerKind || id !== this.pointerId) return undefined;
    if (!this.available || this.busy || normalizeOriginalViewportPoint(xVp, yVp, this.widthVp, this.heightVp) === undefined) {
      this.cancelPointer(); return undefined;
    }
    const dx: number = xVp - this.startX; const dy: number = yVp - this.startY;
    if (dx * dx + dy * dy >= ORIGINAL_VIEWPORT_DRAG_THRESHOLD_VP * ORIGINAL_VIEWPORT_DRAG_THRESHOLD_VP) this.dragged = true;
    return this.dragged ? { offsetX: dx, offsetY: dy } : undefined;
  }
  pointerUp(kind: string, id: number, xVp: number, yVp: number, mode: number): OriginalSelectionRequest | undefined {
    if (this.pointerKind.length === 0 || kind !== this.pointerKind || id !== this.pointerId) return undefined;
    this.pointerMove(kind, id, xVp, yVp);
    const point: OriginalViewportPoint | undefined = normalizeOriginalViewportPoint(xVp, yVp, this.widthVp, this.heightVp);
    const pick: boolean = this.available && !this.busy && this.pointerKind === kind && !this.dragged && point !== undefined;
    this.cancelPointer();
    if (!pick || point === undefined) return undefined;
    return originalSelectionRequest('pick', point.x, point.y, mode);
  }
  keyEvent(key: string, down: boolean, ctrl: boolean, alt: boolean, shift: boolean, meta: boolean): OriginalViewportKeyResult {
    if (!down) this.pressedKeys.delete(key);
    const action: string = key === 'Escape' && !ctrl && !alt && !shift && !meta ? 'clear-selection' :
      key === 'A' && ctrl && !alt && !shift && !meta ? 'select-all' : '';
    if (!this.focused || !this.available || action.length === 0) return { handled: false, action: '' };
    const repeated: boolean = this.pressedKeys.has(key);
    if (down) this.pressedKeys.add(key);
    // Consume focused selection shortcuts while busy, but do not dispatch or
    // turn held-key repeats into extra native selection operations.
    return { handled: true, action: down && !repeated && !this.busy ? action : '' };
  }
}
