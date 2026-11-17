// Ordinary platform transfer only. Returned document URIs remain opaque.
export interface MainFrameFileRequest {
  caption: string;
  filter: string;
  defaultFolderUri: string;
  defaultFileName: string;
  open: boolean;
}
export interface MainFramePickerPlan {
  filters: string[];
  defaultFolderUri: string;
  defaultFileName: string;
}
function field(value: string, maximum: number, label: string): void {
  if (value.length > maximum || value.indexOf('\0') >= 0 || value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0) {
    throw new Error(`${label} must be a bounded single-line value`);
  }
}
export function mainFramePickerPlan(request: MainFrameFileRequest): MainFramePickerPlan {
  field(request.caption, 4096, 'Picker caption');
  field(request.filter, 32768, 'Picker filter');
  field(request.defaultFolderUri, 16384, 'Default folder URI');
  field(request.defaultFileName, 4096, 'Default file name');
  if (request.defaultFolderUri.length > 0 && !/^[A-Za-z][A-Za-z0-9+.-]*:\/\//.test(request.defaultFolderUri)) {
    throw new Error('The default folder must be a document-provider URI, not an invented local path');
  }
  if (request.defaultFileName === '.' || request.defaultFileName === '..' || request.defaultFileName.indexOf('/') >= 0 || request.defaultFileName.indexOf('\\') >= 0) {
    throw new Error('The suggested document name must be a file name, not a path');
  }
  const filters: string[] = [];
  let saveAny: boolean = false;
  if (request.filter.length > 0) {
    const parts: string[] = request.filter.split('|');
    if (parts.length % 2 !== 0 || parts.length / 2 > 100) throw new Error('File filters must contain complete description/mask pairs');
    for (let index: number = 0; index < parts.length; index += 2) {
      const description: string = parts[index].trim();
      const masks: string[] = parts[index + 1].split(';');
      const suffixes: string[] = [];
      for (let mask of masks) {
        mask = mask.trim();
        let suffix: string;
        if (mask === '*' || mask === '*.*') suffix = '.*';
        else {
          if (!/^\*\.[A-Za-z0-9_.+-]+$/.test(mask)) throw new Error('The system document picker cannot represent this file-name glob');
          suffix = mask.substring(1);
        }
        if (suffixes.indexOf(suffix) < 0) suffixes.push(suffix);
      }
      if (suffixes.length === 0) throw new Error('File filter masks cannot be empty');
      if (request.open) filters.push(`${description}|${suffixes.join(',')}`);
      else {
        for (const suffix of suffixes) {
          // The documented save API has one suffix per choice and does not
          // document select()'s .* wildcard. All-files alone uses its default.
          if (suffix === '.*') saveAny = true;
          else filters.push(`${description}|${suffix}`);
        }
        // This is a local bounded-request policy, not an asserted save() API limit.
        if (filters.length > 100) throw new Error('Too many individual save suffix choices');
      }
    }
  }
  if (saveAny && filters.length > 0) {
    throw new Error('The system save picker cannot represent a mixed all-files and suffix-choice menu exactly');
  }
  return { filters, defaultFolderUri: request.defaultFolderUri, defaultFileName: request.defaultFileName };
}
export function mainFrameSelectedUri(uris: string[]): string {
  if (uris.length === 0) return '';
  if (uris.length !== 1 || uris[0].length === 0 || uris[0].length > 65536 || uris[0].indexOf('\0') >= 0) {
    throw new Error('The system picker did not return exactly one document URI');
  }
  if (!/^[A-Za-z][A-Za-z0-9+.-]*:\/\//.test(uris[0]) || /[\r\n]/.test(uris[0])) {
    throw new Error('The picker result must remain a document-provider URI');
  }
  return uris[0];
}
