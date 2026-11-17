// Ordinary desktop focus navigation only. No document transaction or interpreter.
// Key values are verified against the installed InputKit declarations.
export function resolveDesktopTabNavigation(code: number, ctrl: boolean, alt: boolean,
  shift: boolean, meta: boolean): string {
  if (ctrl || alt || meta) return '';
  if (code === 2099 && shift) return 'context'; // Shift+F10
  if (shift) return '';
  switch (code) {
    case 2014: return 'previous'; // KEYCODE_DPAD_LEFT
    case 2015: return 'next'; // KEYCODE_DPAD_RIGHT
    case 2081: return 'first'; // KEYCODE_MOVE_HOME, not the system Home key
    case 2082: return 'last'; // KEYCODE_MOVE_END
    case 2067: return 'context'; // KEYCODE_MENU
    default: return '';
  }
}

export function desktopTabFocusTarget(ids: string[], current: string, action: string): string {
  if (ids.length === 0 || ids.length > 4 || ids.some((id: string): boolean => id.length === 0) ||
    new Set<string>(ids).size !== ids.length || ids.indexOf(current) < 0) return '';
  if (action === 'first') return ids[0];
  if (action === 'last') return ids[ids.length - 1];
  if (action !== 'previous' && action !== 'next') return '';
  return ids[(ids.indexOf(current) + (action === 'previous' ? ids.length - 1 : 1)) % ids.length];
}
