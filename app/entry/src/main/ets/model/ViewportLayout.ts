// Ordinary window layout only. These are allocation budgets in vp, never native pixel dimensions.
export interface ViewportLayoutBudget {
  compact: boolean;
  workspaceGap: number;
  workspacePadding: number;
  panelMeasured: boolean;
  panelHeightBudget: number;
  toolsHeight: number;
  surfaceHeightBudget: number;
}

export function viewportLayoutBudget(desktopHeight: number, panelHeight: number,
  expanded: boolean): ViewportLayoutBudget {
  const desktopValid: boolean = Number.isFinite(desktopHeight) && desktopHeight > 0;
  const panelMeasured: boolean = Number.isFinite(panelHeight) && panelHeight > 0;
  const compact: boolean = !desktopValid || desktopHeight < 760;
  // Before the panel is measured, use only a conservative layout estimate.
  // Once measured, the actual panel height is the sole vertical allocation bound.
  const panelHeightBudget: number = panelMeasured ? panelHeight :
    desktopValid ? Math.max(0, desktopHeight - 112) : 0;
  const remaining: number = Math.max(0, panelHeightBudget - 50); // 32 header + 16 status + 2 border.
  const desiredTools: number = expanded ? 224 : compact ? 72 : 104;
  // Reserve a useful 160vp surface whenever a 34vp scrollable tools strip also fits.
  // Truly tiny windows share the available area; no minHeight can force overflow.
  const toolsHeight: number = remaining >= 194 ? Math.min(desiredTools, remaining - 160) :
    Math.min(desiredTools, 34, remaining * 0.25);
  return {
    compact: compact,
    workspaceGap: compact ? 6 : 18,
    workspacePadding: compact ? 8 : 24,
    panelMeasured: panelMeasured,
    panelHeightBudget: panelHeightBudget,
    toolsHeight: toolsHeight,
    surfaceHeightBudget: remaining - toolsHeight
  };
}
