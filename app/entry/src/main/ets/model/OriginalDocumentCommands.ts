// Platform command routing only; never invokes an application interpreter.
// Key values are checked against the installed InputKit keyCode declaration.
export interface OriginalDocumentShortcutContext {
  enabled: boolean;
  editorVisible: boolean;
  sourceFocused: boolean;
  findVisible: boolean;
  goToLineVisible?: boolean;
  tabCount?: number;
}

export function resolveOriginalDocumentShortcut(code: number, ctrl: boolean,
  alt: boolean, shift: boolean, meta: boolean, context: OriginalDocumentShortcutContext): string {
  if (!context.enabled || alt || meta) return '';
  if (!ctrl && !shift && code === 2099) return 'desktop-menu-focus'; // F10: focus, not activation.
  if (ctrl) {
    // Ctrl+Tab cycles documents, while plain Tab/Shift+Tab stay with the focused control.
    if (code === 2049 && context.tabCount !== undefined && context.tabCount > 1) {
      return shift ? 'document-previous-tab' : 'document-next-tab';
    }
    if (shift) {
      if (code === 2031) return 'options-import'; // Ctrl+Shift+O
      if (code === 2035) return 'options-export'; // Ctrl+Shift+S
      if (context.editorVisible && context.sourceFocused && code === 2042) return 'text-redo';
      return '';
    }
    switch (code) {
      case 2031: return 'document-open';
      case 2035: return 'document-save';
      case 2039: return 'document-close';
      case 2021: return 'document-edit';
    }
    if (!context.editorVisible) return '';
    if (code === 2022) return 'text-find'; // Ctrl+F
    if (code === 2024) return 'text-replace'; // Ctrl+H focuses the replacement field.
    if (code === 2023) return 'text-go-to-line'; // Ctrl+G moves the caret only.
    if (!context.sourceFocused) return '';
    if (code === 2042) return 'text-undo';
    if (code === 2041) return 'text-redo';
    return '';
  }
  if (!context.editorVisible) return '';
  if (code === 2092 && context.findVisible) return shift ? 'text-find-previous' : 'text-find-next'; // Shift+F3 / F3
  if (shift) return '';
  if (code === 2070 && context.goToLineVisible) return 'text-go-to-line-close';
  if (code === 2070 && context.findVisible) return 'text-find-close';
  // Ctrl+A, clipboard, ordinary typing and Escape outside Find belong to the
  // focused control. Document close always requires Ctrl+W or an explicit UI.
  return '';
}
