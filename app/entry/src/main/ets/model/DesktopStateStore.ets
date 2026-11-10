import { fileIo } from '@kit.CoreFileKit';
import { DesktopState, defaultDesktopState, parseDesktopState } from './DesktopState';

export interface DesktopStateLoad {
  state: DesktopState;
  status: string;
}

export function loadDesktopState(directory: string): DesktopStateLoad {
  const fallback: DesktopState = defaultDesktopState();
  if (directory.length === 0) return { state: fallback, status: 'directory-unavailable' };
  const path: string = directory + '/desktop-view-state.json';
  try {
    if (!fileIo.accessSync(path)) return { state: fallback, status: 'missing' };
    const size: number = fileIo.statSync(path).size;
    if (size <= 0 || size > 4096) return { state: fallback, status: 'invalid' };
    const state: DesktopState | undefined = parseDesktopState(fileIo.readTextSync(path));
    return state === undefined ? { state: fallback, status: 'invalid' } : { state: state, status: 'restored' };
  } catch (error) {
    return { state: fallback, status: 'read-error' };
  }
}

export function writeDesktopState(directory: string, state: DesktopState): boolean {
  if (directory.length === 0) return false;
  const serialized: string = JSON.stringify(state);
  if (parseDesktopState(serialized) === undefined) return false;
  const path: string = directory + '/desktop-view-state.json';
  const temporary: string = path + '.tmp';
  try {
    const file = fileIo.openSync(temporary,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
    try {
      if (fileIo.writeSync(file.fd, serialized) !== serialized.length) return false;
      fileIo.fsyncSync(file.fd);
    } finally {
      fileIo.closeSync(file);
    }
    // Replace only after complete write/close; preserve the last file on failure.
    fileIo.renameSync(temporary, path);
    return true;
  } catch (error) {
    return false;
  }
}
