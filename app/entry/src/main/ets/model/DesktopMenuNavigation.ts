// Platform menu metadata only: no application commands, parsing or document edits.
const MENU_IDS: string[] = ['olex2-menu-file', 'olex2-menu-view', 'olex2-menu-structure',
  'olex2-menu-help', 'olex2-sidebar-toggle'];

export function desktopMenuButtonWidth(id: string): number {
  return MENU_IDS.indexOf(id) < 0 ? 0 : id === 'olex2-sidebar-toggle' ? 88 : 56;
}

export function enabledDesktopMenuIds(busy: boolean, editorVisible: boolean,
  documentPresented: boolean): string[] {
  if (busy) return [];
  return MENU_IDS.filter((id: string): boolean => {
    if (id === 'olex2-menu-view' || id === 'olex2-menu-structure') return !editorVisible;
    return true;
  });
}

export function resolveDesktopMenuNavigation(code: number, ctrl: boolean, alt: boolean,
  shift: boolean, meta: boolean): string {
  if (ctrl || alt || shift || meta) return '';
  switch (code) {
    case 2014: return 'previous'; // KEYCODE_DPAD_LEFT
    case 2015: return 'next'; // KEYCODE_DPAD_RIGHT
    case 2081: return 'first'; // KEYCODE_MOVE_HOME, not the system Home key.
    case 2082: return 'last'; // KEYCODE_MOVE_END
    case 2070: return 'return'; // KEYCODE_ESCAPE: focus only, never close a document.
    default: return '';
  }
}

export function desktopMenuFocusTarget(ids: string[], current: string, action: string): string {
  if (ids.length === 0 || new Set<string>(ids).size !== ids.length ||
    ids.some((id: string): boolean => MENU_IDS.indexOf(id) < 0)) return '';
  if (action === 'first') return ids[0];
  if (action === 'last') return ids[ids.length - 1];
  if (action !== 'previous' && action !== 'next') return '';
  const index: number = ids.indexOf(current);
  if (index < 0) return action === 'previous' ? ids[ids.length - 1] : ids[0];
  return ids[(index + (action === 'previous' ? ids.length - 1 : 1)) % ids.length];
}

export function desktopMenuScrollOffset(id: string, viewportWidth: number,
  currentOffset: number): number | undefined {
  const index: number = MENU_IDS.indexOf(id);
  if (index < 0 || !Number.isFinite(viewportWidth) || viewportWidth <= 0) return undefined;
  let contentWidth: number = 0;
  let start: number = 0;
  for (let i: number = 0; i < MENU_IDS.length; i++) {
    if (i === index) start = contentWidth;
    contentWidth += desktopMenuButtonWidth(MENU_IDS[i]) + (i + 1 < MENU_IDS.length ? 4 : 0);
  }
  const width: number = desktopMenuButtonWidth(id);
  const maximum: number = Math.max(0, contentWidth - viewportWidth);
  const current: number = Math.min(maximum, Math.max(0, Number.isFinite(currentOffset) ? currentOffset : 0));
  const target: number = viewportWidth < width || start < current ? start :
    start + width > current + viewportWidth ? start + width - viewportWidth : current;
  return Math.min(maximum, Math.max(0, target));
}
