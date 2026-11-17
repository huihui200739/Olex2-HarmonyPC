// Ordinary PC document navigation only. No parsing, saving or interpreter execution.
// Return a stable id; the owner still performs its existing activation transaction.
export function adjacentDesktopDocumentId(ids: string[], activeId: string, direction: number): string {
  if ((direction !== 1 && direction !== -1) || ids.length < 2 || activeId.length === 0) return '';
  const seen: Set<string> = new Set<string>();
  for (const id of ids) {
    if (id.length === 0 || seen.has(id)) return '';
    seen.add(id);
  }
  const index: number = ids.indexOf(activeId);
  if (index < 0) return '';
  return ids[(index + direction + ids.length) % ids.length];
}

export function desktopDocumentMenuLabel(name: string, active: boolean,
  draftDirty: boolean, documentDirty: boolean): string {
  const label: string = name.length > 0 ? name : '未命名文档';
  const dirty: string = draftDirty && documentDirty ? ' · 待应用 / 未保存' :
    draftDirty ? ' · 待应用' : documentDirty ? ' · 未保存' : '';
  return (active ? '✓ ' : '') + label + dirty;
}
