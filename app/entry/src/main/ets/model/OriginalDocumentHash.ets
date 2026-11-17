import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { validateOriginalDocumentBytes } from './OriginalDocumentTransfer';

// Ordinary file fingerprint only; no source parsing or research computation.
// The installed SDK exposes synchronous SHA256 message digest since API12.
export function originalDocumentSha256(bytes: Uint8Array): string {
  validateOriginalDocumentBytes(bytes);
  const digest: cryptoFramework.Md = cryptoFramework.createMd('SHA256');
  digest.updateSync({ data: bytes });
  const data: Uint8Array = digest.digestSync().data;
  if (data.byteLength !== 32) throw new Error('系统文件摘要长度不一致，未开始写入');
  let hex: string = '';
  for (let i: number = 0; i < data.byteLength; i++) hex += data[i].toString(16).padStart(2, '0');
  return hex;
}
