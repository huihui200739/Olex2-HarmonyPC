// Ordinary UI text only. All offsets and retained sizes are UTF-16 code units.
// No validation/normalization of text: transient IME strings must round-trip.
export interface OriginalTextHistoryStatus {
  canUndo: boolean;
  canRedo: boolean;
  undoDepth: number;
  redoDepth: number;
  retainedCharacters: number;
  maxCharacters: number;
  maxSteps: number;
}

export interface OriginalTextPatch {
  start: number;
  removed: string;
  inserted: string;
}

// Arrays use the internal stack order: the LAST item is the next undo/redo.
export interface OriginalTextHistoryRecovery {
  current: string;
  undoPatches: OriginalTextPatch[];
  redoPatches: OriginalTextPatch[];
  maxSteps: number;
  maxCharacters: number;
}

const DEFAULT_HISTORY_STEPS: number = 64;
const DEFAULT_HISTORY_CHARACTERS: number = 1024 * 1024;
const DEFAULT_REPLACEMENT_CHARACTERS: number = 8 * 1024 * 1024;

function requireTextLimit(value: number): number {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new RangeError('Text limit must be a non-negative safe integer');
  }
  return value;
}

function requireHistoryShape(value: object, fields: string[]): void {
  if (value === null || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error('History recovery must be an object');
  }
  const keys: string[] = Object.keys(value);
  if (keys.length !== fields.length) throw new Error('History recovery fields are incomplete');
  for (let i: number = 0; i < keys.length; i++) {
    if (fields.indexOf(keys[i]) < 0) throw new Error('Unexpected history recovery field');
  }
}

function copyRecoveryPatches(patches: OriginalTextPatch[], maximum: number): OriginalTextPatch[] {
  const copies: OriginalTextPatch[] = [];
  for (let i: number = 0; i < patches.length; i++) {
    const patch: OriginalTextPatch = patches[i];
    requireHistoryShape(patch, ['start', 'removed', 'inserted']);
    if (!Number.isSafeInteger(patch.start) || patch.start < 0 ||
      typeof patch.removed !== 'string' || typeof patch.inserted !== 'string' ||
      patch.removed.length > maximum || patch.inserted.length > maximum || patch.removed === patch.inserted) {
      throw new Error('Invalid history recovery patch');
    }
    copies.push({ start: patch.start, removed: patch.removed, inserted: patch.inserted });
  }
  return copies;
}

function checkRecoveryPath(current: string, patches: OriginalTextPatch[], undo: boolean): void {
  let text: string = current;
  for (let i: number = patches.length - 1; i >= 0; i--) {
    const patch: OriginalTextPatch = patches[i];
    const removed: string = undo ? patch.inserted : patch.removed;
    const inserted: string = undo ? patch.removed : patch.inserted;
    if (patch.start > text.length || removed.length > text.length - patch.start ||
      text.slice(patch.start, patch.start + removed.length) !== removed ||
      text.length - removed.length + inserted.length > DEFAULT_REPLACEMENT_CHARACTERS) {
      throw new Error('History recovery patch does not apply to its actual text');
    }
    text = text.slice(0, patch.start) + inserted + text.slice(patch.start + removed.length);
  }
}

export function validateOriginalTextHistoryRecovery(snapshot: OriginalTextHistoryRecovery): OriginalTextHistoryRecovery {
  requireHistoryShape(snapshot, ['current', 'undoPatches', 'redoPatches', 'maxSteps', 'maxCharacters']);
  const steps: number = requireTextLimit(snapshot.maxSteps);
  const characters: number = requireTextLimit(snapshot.maxCharacters);
  if (typeof snapshot.current !== 'string' || snapshot.current.length > DEFAULT_REPLACEMENT_CHARACTERS ||
    !Array.isArray(snapshot.undoPatches) || !Array.isArray(snapshot.redoPatches) ||
    snapshot.undoPatches.length > steps || snapshot.redoPatches.length > steps - snapshot.undoPatches.length) {
    throw new Error('History recovery text or step capacity exceeded');
  }
  // Count before copying or replaying. Every real patch retains at least one
  // character. Configured limits are preserved, never silently replaced by defaults.
  let retained: number = 0;
  const groups: OriginalTextPatch[][] = [snapshot.undoPatches, snapshot.redoPatches];
  for (let group: number = 0; group < groups.length; group++) {
    for (let i: number = 0; i < groups[group].length; i++) {
      const patch: OriginalTextPatch = groups[group][i];
      requireHistoryShape(patch, ['start', 'removed', 'inserted']);
      if (typeof patch.removed !== 'string' || typeof patch.inserted !== 'string') {
        throw new Error('History patch text is missing');
      }
      const size: number = patch.removed.length + patch.inserted.length;
      if (!Number.isSafeInteger(size) || size > characters - retained) {
        throw new Error('History recovery character capacity exceeded');
      }
      retained += size;
    }
  }
  const undo: OriginalTextPatch[] = copyRecoveryPatches(snapshot.undoPatches, DEFAULT_REPLACEMENT_CHARACTERS);
  const redo: OriginalTextPatch[] = copyRecoveryPatches(snapshot.redoPatches, DEFAULT_REPLACEMENT_CHARACTERS);
  checkRecoveryPath(snapshot.current, undo, true);
  checkRecoveryPath(snapshot.current, redo, false);
  return { current: snapshot.current, undoPatches: undo, redoPatches: redo,
    maxSteps: steps, maxCharacters: characters };
}

export class OriginalTextHistory {
  private current: string = '';
  private undoPatches: OriginalTextPatch[] = [];
  private redoPatches: OriginalTextPatch[] = [];
  private retainedCharacters: number = 0;
  private maxStepCount: number;
  private maxCharacterCount: number;

  constructor(maxSteps: number = DEFAULT_HISTORY_STEPS,
    maxCharacters: number = DEFAULT_HISTORY_CHARACTERS) {
    this.maxStepCount = requireTextLimit(maxSteps);
    this.maxCharacterCount = requireTextLimit(maxCharacters);
  }

  reset(text: string): void {
    this.current = text;
    this.clearPatches();
  }

  record(text: string): void {
    // A UI echo of the current value must not retire an available redo branch.
    if (text === this.current) return;
    for (let i: number = 0; i < this.redoPatches.length; i++) {
      this.retainedCharacters -= this.patchCharacters(this.redoPatches[i]);
    }
    this.redoPatches = [];

    let start: number = 0;
    const commonLength: number = Math.min(this.current.length, text.length);
    while (start < commonLength && this.current.charCodeAt(start) === text.charCodeAt(start)) {
      start++;
    }
    let oldEnd: number = this.current.length;
    let newEnd: number = text.length;
    while (oldEnd > start && newEnd > start &&
      this.current.charCodeAt(oldEnd - 1) === text.charCodeAt(newEnd - 1)) {
      oldEnd--;
      newEnd--;
    }
    const characters: number = oldEnd - start + newEnd - start;
    if (this.maxStepCount === 0 || characters > this.maxCharacterCount) {
      // Check before slicing: an over-budget edit never allocates a large patch.
      this.current = text;
      this.clearPatches();
      return;
    }
    const patch: OriginalTextPatch = {
      start: start,
      removed: this.current.slice(start, oldEnd),
      inserted: text.slice(start, newEnd)
    };
    this.current = text;
    this.undoPatches.push(patch);
    this.retainedCharacters += characters;
    while (this.undoPatches.length > this.maxStepCount ||
      this.retainedCharacters > this.maxCharacterCount) {
      const oldest: OriginalTextPatch | undefined = this.undoPatches.shift();
      if (oldest !== undefined) this.retainedCharacters -= this.patchCharacters(oldest);
    }
  }

  undo(): string | undefined {
    const patch: OriginalTextPatch | undefined = this.undoPatches.pop();
    if (patch === undefined) return undefined;
    this.current = this.current.slice(0, patch.start) + patch.removed +
      this.current.slice(patch.start + patch.inserted.length);
    this.redoPatches.push(patch);
    return this.current;
  }

  redo(): string | undefined {
    const patch: OriginalTextPatch | undefined = this.redoPatches.pop();
    if (patch === undefined) return undefined;
    this.current = this.current.slice(0, patch.start) + patch.inserted +
      this.current.slice(patch.start + patch.removed.length);
    this.undoPatches.push(patch);
    return this.current;
  }

  exportRecovery(): OriginalTextHistoryRecovery {
    return validateOriginalTextHistoryRecovery({ current: this.current,
      undoPatches: this.undoPatches, redoPatches: this.redoPatches,
      maxSteps: this.maxStepCount, maxCharacters: this.maxCharacterCount });
  }

  restoreRecovery(snapshot: OriginalTextHistoryRecovery): void {
    const checked: OriginalTextHistoryRecovery = validateOriginalTextHistoryRecovery(snapshot);
    let retained: number = 0;
    for (let i: number = 0; i < checked.undoPatches.length; i++) retained += this.patchCharacters(checked.undoPatches[i]);
    for (let i: number = 0; i < checked.redoPatches.length; i++) retained += this.patchCharacters(checked.redoPatches[i]);
    // All validation/replay completed against copies before changing any state.
    this.current = checked.current;
    this.undoPatches = checked.undoPatches;
    this.redoPatches = checked.redoPatches;
    this.maxStepCount = checked.maxSteps;
    this.maxCharacterCount = checked.maxCharacters;
    this.retainedCharacters = retained;
  }

  getStatus(): OriginalTextHistoryStatus {
    return {
      canUndo: this.undoPatches.length > 0,
      canRedo: this.redoPatches.length > 0,
      undoDepth: this.undoPatches.length,
      redoDepth: this.redoPatches.length,
      retainedCharacters: this.retainedCharacters,
      maxCharacters: this.maxCharacterCount,
      maxSteps: this.maxStepCount
    };
  }

  private patchCharacters(patch: OriginalTextPatch): number {
    return patch.removed.length + patch.inserted.length;
  }

  private clearPatches(): void {
    this.undoPatches = [];
    this.redoPatches = [];
    this.retainedCharacters = 0;
  }
}

export interface LiteralTextMatch {
  start: number;
  end: number;
  wrapped: boolean;
}

export interface LiteralTextReplacement {
  text: string;
  start: number;
  end: number;
  replacements: number;
}

export function findLiteralText(text: string, query: string, from: number):
  LiteralTextMatch | undefined {
  if (query.length === 0) return undefined;
  // NaN/negative offsets begin at zero; +Infinity clamps to the text end.
  let offset: number = 0;
  if (!Number.isNaN(from) && from > 0) offset = Math.min(Math.floor(from), text.length);
  let start: number = text.indexOf(query, offset);
  let wrapped: boolean = false;
  if (start < 0 && offset > 0) {
    start = text.indexOf(query);
    wrapped = true;
  }
  if (start < 0) return undefined;
  return { start: start, end: start + query.length, wrapped: wrapped };
}

// Reverse search ends at or before `from`, excluding the currently selected match.
// Both directions use UTF-16 offsets and wrap without changing the draft.
export function findPreviousLiteralText(text: string, query: string, from: number):
  LiteralTextMatch | undefined {
  if (query.length === 0) return undefined;
  let offset: number = 0;
  if (!Number.isNaN(from) && from > 0) offset = Math.min(Math.floor(from), text.length);
  let start: number = offset >= query.length ? text.lastIndexOf(query, offset - query.length) : -1;
  let wrapped: boolean = false;
  if (start < 0) {
    start = text.lastIndexOf(query);
    wrapped = true;
  }
  if (start < 0) return undefined;
  return { start: start, end: start + query.length, wrapped: wrapped };
}

export interface LiteralTextLineTarget {
  // Zero denotes invalid input; start=-1 denotes a missing/invalid line.
  line: number;
  lineCount: number;
  start: number;
}

// Logical lines only: LF, CRLF and lone CR. Soft wrapping does not add lines.
// Scan without splitting/copying the draft; offsets remain UTF-16 code units.
export function literalTextLineTarget(text: string, input: string): LiteralTextLineTarget {
  const value: string = input.trim();
  const requested: number = /^[0-9]+$/.test(value) ? Number(value) : 0;
  const line: number = Number.isSafeInteger(requested) && requested > 0 ? requested : 0;
  let lineCount: number = 1;
  let start: number = line === 1 ? 0 : -1;
  for (let i: number = 0; i < text.length; i++) {
    const code: number = text.charCodeAt(i);
    if (code !== 10 && code !== 13) continue;
    if (code === 13 && i + 1 < text.length && text.charCodeAt(i + 1) === 10) i++;
    lineCount++;
    if (lineCount === line) start = i + 1;
  }
  return { line: line, lineCount: lineCount, start: start };
}

export function replaceLiteralMatch(text: string, query: string, replacement: string,
  match: LiteralTextMatch): LiteralTextReplacement | undefined {
  if (!match || query.length === 0 || !Number.isInteger(match.start) ||
    !Number.isInteger(match.end) || typeof match.wrapped !== 'boolean' ||
    match.start < 0 || match.end > text.length || match.end - match.start !== query.length ||
    text.slice(match.start, match.end) !== query) {
    return undefined;
  }
  return {
    text: text.slice(0, match.start) + replacement + text.slice(match.end),
    start: match.start,
    end: match.start + replacement.length,
    replacements: 1
  };
}

export function replaceAllLiteralText(text: string, query: string, replacement: string,
  maxLength: number = DEFAULT_REPLACEMENT_CHARACTERS): LiteralTextReplacement {
  // An empty query or a miss returns the original string without allocation.
  if (query.length === 0) return { text: text, start: 0, end: 0, replacements: 0 };
  const limit: number = requireTextLimit(maxLength);
  const growth: number = replacement.length - query.length;
  let count: number = 0;
  let first: number = -1;
  let start: number = text.indexOf(query);
  // First pass counts only; no replacement pieces or output are allocated yet.
  while (start >= 0) {
    if (first < 0) first = start;
    count++;
    // Division avoids a potentially overflowing expansion multiplication.
    if (growth > 0 && (text.length > limit || count > Math.floor((limit - text.length) / growth))) {
      throw new RangeError('Literal replacement exceeds maximum text length');
    }
    start = text.indexOf(query, start + query.length);
  }
  if (count === 0) return { text: text, start: 0, end: 0, replacements: 0 };
  const outputLength: number = text.length + count * growth;
  if (outputLength > limit) throw new RangeError('Literal replacement exceeds maximum text length');
  if (query === replacement) {
    return { text: text, start: first, end: first + replacement.length, replacements: count };
  }
  const pieces: string[] = [];
  let from: number = 0;
  start = first;
  while (start >= 0) {
    // Skip empty pieces so deletion-heavy input cannot create a huge empty array.
    if (start > from) pieces.push(text.slice(from, start));
    if (replacement.length > 0) pieces.push(replacement);
    from = start + query.length;
    start = text.indexOf(query, from);
  }
  if (from < text.length) pieces.push(text.slice(from));
  return {
    text: pieces.join(''), start: first, end: first + replacement.length, replacements: count
  };
}
