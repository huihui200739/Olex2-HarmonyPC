// Versioned preferences for diagnostic geometry only; no model/user document data.
export interface DesktopState {
  schemaVersion: number;
  primitive: number;
  colorPreset: number;
  materialPreset: number;
  generation: number;
  wireframe: boolean;
  yaw: number;
  pitch: number;
  distance: number;
}

export function defaultDesktopState(): DesktopState {
  return { schemaVersion: 4, primitive: 0, colorPreset: 0, materialPreset: 0, generation: 3, wireframe: false,
    yaw: 32, pitch: -22, distance: 4.4 };
}

export function parseDesktopState(text: string): DesktopState | undefined {
  if (text.length === 0 || text.length > 4096) return undefined;
  try {
    const state: DesktopState | null = JSON.parse(text) as DesktopState | null;
    if (state === null || typeof state !== 'object' || Array.isArray(state) ||
      (state.schemaVersion !== 1 && state.schemaVersion !== 2 && state.schemaVersion !== 3 && state.schemaVersion !== 4) ||
      (state.schemaVersion >= 2 && (!Number.isInteger(state.colorPreset) || state.colorPreset < 0 || state.colorPreset > 2)) ||
      (state.schemaVersion >= 3 && (!Number.isInteger(state.materialPreset) || state.materialPreset < 0 || state.materialPreset > (state.schemaVersion === 3 ? 2 : 3))) ||
      !Number.isInteger(state.primitive) || state.primitive < 0 || state.primitive > 3 ||
      !Number.isInteger(state.generation) || state.generation < 0 || state.generation > 5 ||
      typeof state.wireframe !== 'boolean' ||
      !Number.isFinite(state.yaw) || state.yaw < -360 || state.yaw > 360 ||
      !Number.isFinite(state.pitch) || state.pitch < -85 || state.pitch > 85 ||
      !Number.isFinite(state.distance) || state.distance < 2.2 || state.distance > 10) return undefined;
    // Return only our versioned fields, not unknown metadata from the file.
    return { schemaVersion: 4, materialPreset: state.schemaVersion < 3 ? 0 : state.materialPreset, colorPreset: state.schemaVersion === 1 ? 0 : state.colorPreset, primitive: state.primitive, generation: state.generation,
      wireframe: state.wireframe, yaw: state.yaw, pitch: state.pitch, distance: state.distance };
  } catch (error) {
    return undefined;
  }
}

// Pure coalescing policy: timers and file access remain in the ArkUI adapter.
export class DesktopStateSaveQueue {
  private savedKey: string = '';
  private failedKey: string = '';
  private pendingKey: string = '';
  private pending: DesktopState | undefined = undefined;

  stage(state: DesktopState): string {
    const key: string = JSON.stringify(state);
    if (key === this.savedKey || key === this.failedKey) {
      this.pending = undefined;
      this.pendingKey = '';
      return key === this.savedKey ? 'saved' : 'write-error';
    }
    if (key === this.pendingKey) return 'pending';
    this.pending = state;
    this.pendingKey = key;
    return 'changed';
  }

  pendingState(): DesktopState | undefined { return this.pending; }

  complete(saved: boolean): void {
    if (this.pending === undefined) return;
    if (saved) {
      this.savedKey = this.pendingKey;
      this.failedKey = '';
    } else {
      this.failedKey = this.pendingKey;
    }
    this.pending = undefined;
    this.pendingKey = '';
  }
}
