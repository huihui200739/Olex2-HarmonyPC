// Platform UI allocation in vp only. No document parsing, editing or native work.
export interface DesktopEditorLayoutBudget {
  compact: boolean;
  narrow: boolean;
  padding: number;
  gap: number;
  headerHeight: number;
  toolsHeight: number;
  textHeight: number;
  statusHeight: number;
}

export function desktopEditorLayoutBudget(width: number, height: number,
  findVisible: boolean): DesktopEditorLayoutBudget {
  const w: number = Number.isFinite(width) && width > 0 ? width : 0;
  const h: number = Number.isFinite(height) && height > 0 ? height : 0;
  const compact: boolean = w < 720 || h < 640;
  const padding: number = Math.min(compact ? 8 : 16, w / 2, h / 2);
  const innerHeight: number = Math.max(0, h - padding * 2);
  const narrow: boolean = w - padding * 2 < 620;
  const headerHeight: number = Math.min(compact ? 44 : 52, innerHeight * 0.24);
  const statusHeight: number = Math.min(compact ? 22 : 36, innerHeight * 0.12);
  const gap: number = Math.min(compact ? 6 : 10,
    Math.max(0, innerHeight - headerHeight - statusHeight) / 3);
  const remaining: number = Math.max(0, innerHeight - headerHeight - statusHeight - gap * 3);
  // Even when find/replace expands, tools scroll instead of consuming the draft.
  const desiredTools: number = findVisible ? (narrow ? 324 : 224) : (compact ? 148 : 188);
  const toolsHeight: number = Math.min(desiredTools, remaining * 0.45);
  return { compact: compact, narrow: narrow, padding: padding, gap: gap,
    headerHeight: headerHeight, toolsHeight: toolsHeight,
    textHeight: remaining - toolsHeight, statusHeight: statusHeight };
}

export const DESKTOP_DOCUMENT_TAB_WIDTH: number = 236;
export const DESKTOP_DOCUMENT_TAB_GAP: number = 6;
export const DESKTOP_DOCUMENT_TAB_PADDING: number = 8;

// Reveal the selected tab with the smallest necessary horizontal movement.
// Undefined means unmeasured/invalid metadata; callers must not scroll yet.
export function desktopDocumentTabOffset(index: number, count: number,
  viewportWidth: number, currentOffset: number): number | undefined {
  if (!Number.isSafeInteger(index) || !Number.isSafeInteger(count) || count <= 0 ||
    index < 0 || index >= count || !Number.isFinite(viewportWidth) || viewportWidth <= 0) return undefined;
  const contentWidth: number = count * DESKTOP_DOCUMENT_TAB_WIDTH + (count - 1) * DESKTOP_DOCUMENT_TAB_GAP;
  if (!Number.isFinite(contentWidth)) return undefined;
  const maximum: number = Math.max(0, contentWidth - viewportWidth);
  const current: number = Math.min(maximum, Math.max(0, Number.isFinite(currentOffset) ? currentOffset : 0));
  const start: number = index * (DESKTOP_DOCUMENT_TAB_WIDTH + DESKTOP_DOCUMENT_TAB_GAP);
  const end: number = start + DESKTOP_DOCUMENT_TAB_WIDTH;
  const target: number = viewportWidth < DESKTOP_DOCUMENT_TAB_WIDTH || start < current ? start :
    end > current + viewportWidth ? end - viewportWidth : current;
  return Math.min(maximum, Math.max(0, target));
}
