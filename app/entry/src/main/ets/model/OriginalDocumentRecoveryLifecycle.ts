// Synchronous lifecycle handoff only; the page owns validation and checkpoint IO.
let recoveryFlush: (() => void) | undefined = undefined;
let registration: number = 0;
export function registerOriginalDocumentRecoveryFlush(callback: () => void): number {
  registration++; recoveryFlush = callback; return registration;
}
export function unregisterOriginalDocumentRecoveryFlush(token: number): void {
  if (token === registration) recoveryFlush = undefined;
}
export function flushOriginalDocumentRecoveryBeforePause(): void {
  if (recoveryFlush !== undefined) recoveryFlush();
}
