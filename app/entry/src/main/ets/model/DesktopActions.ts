// Platform-only desktop dispatch. Numeric keys follow the installed InputKit SDK.
// No application interpreter, native renderer or document parser executes here.
export function resolveDesktopShortcut(code: number, ctrl: boolean, alt: boolean,
  shift: boolean, meta: boolean): string {
  if (!ctrl || alt || shift || meta) return '';
  switch (code) {
    case 2031: return 'options-import'; // O: explicitly settings, not structure documents.
    case 2035: return 'options-export'; // S: opens the existing system save picker.
    case 2028: return 'focus-command'; // L
    case 2018: return 'toggle-sidebar'; // B
    case 2000: return 'reset-view';
    case 2001: return 'panel-0';
    case 2002: return 'panel-1';
    case 2003: return 'panel-2';
    default: return '';
  }
}

export class DesktopShortcutLatch {
  private held: Set<number> = new Set<number>();
  press(code: number): boolean {
    if (this.held.has(code)) return false;
    this.held.add(code);
    return true;
  }
  release(code: number): void { this.held.delete(code); }
  reset(): void { this.held.clear(); }
}

export function desktopContentWidth(windowWidth: number, sidebar: boolean, height: number = 900): number {
  const width: number = Number.isFinite(windowWidth) && windowWidth > 0 ? windowWidth : 1200;
  const padding: number = !Number.isFinite(height) || height < 760 ? 8 : 24;
  return Math.max(0, width - padding * 2 - (sidebar ? 216 : 0));
}

export function compactDesktopSummary(width: number, sidebar: boolean, height: number): boolean {
  return desktopContentWidth(width, sidebar, height) < 760 || !Number.isFinite(height) || height < 820;
}

export function showDesktopChecksPanel(width: number, sidebar: boolean, height: number): boolean {
  return desktopContentWidth(width, sidebar, height) >= 820 && Number.isFinite(height) && height >= 760;
}

// Keep compact branding separate from the user's explicit sidebar visibility preference.
export function compactDesktopSidebar(height: number): boolean {
  return !Number.isFinite(height) || height < 760;
}

// A scrollable help strip must not consume the entire short workspace.
export function desktopHelpHeight(height: number): number {
  if (!Number.isFinite(height) || height <= 0) return 56;
  return Math.min(144, height * 0.25, Math.max(56, height * 0.22));
}
