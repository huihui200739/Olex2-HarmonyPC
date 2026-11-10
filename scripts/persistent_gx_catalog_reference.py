"""Parse a genuine GX CPU metadata run, never fabricate an old TX smoke result."""
import hashlib,re
COUNTS={'macros':196,'functions':52}
ROOT_COUNTS={'macros':193,'functions':46}
HOST_ASSERTIONS=276
MARKER=f'PASS genuine GX metadata host; {HOST_ASSERTIONS} assertions; root=193/46; recursive=196/52; no loaded data, GL context, fonts or full application qualification'
def parse_catalog_log(text):
 if text.splitlines().count(MARKER)!=1:raise ValueError('Missing exact genuine GX metadata host completion')
 result={}
 for kind,count in COUNTS.items():
  rows=re.findall(r'^GX_CATALOG_LINE '+kind+r' (\d+) ([0-9a-f]+)$',text,re.M)
  if len(rows)!=count or [int(index) for index,_ in rows]!=list(range(count)):raise ValueError('Original recursive GX catalog ordinal inventory differs')
  try:pieces=[bytes.fromhex(value).decode('utf-8') for _,value in rows]
  except (ValueError,UnicodeError) as e:raise ValueError('Invalid original GX catalog byte framing') from e
  if not all(re.fullmatch(r'[^\n\x00]+ - [^\n\x00]+\n',line) for line in pieces):raise ValueError('Original GX signature framing differs')
  raw=''.join(pieces).encode('utf-8');digest=hashlib.sha256(raw).hexdigest()
  if re.findall(r'^GX_CATALOG_DIGEST '+kind+r' (\d+) (\d+) ([0-9a-f]{64})$',text,re.M)!=[(str(count),str(len(raw)),digest)]:raise ValueError('Original GX catalog digest/bytes differ')
  observation=re.findall(r'^GX_CATALOG_OBSERVATION '+kind+r' root=(\d+) recursive=(\d+) entries=(\d+) posted=(\d+) dropped=(\d+) bytes=(\d+) truncated=(\d+)$',text,re.M)
  if observation!=[(str(ROOT_COUNTS[kind]),str(count),str(count),str(count),'0',str(len(raw)),'0')]:raise ValueError('Original root/recursive counts or bounded notifications differ')
  result[kind]=dict(text=raw.decode('utf-8'),sha256=digest,entry_count=count,bytes=len(raw),first=pieces[0],last=pieces[-1])
 return result
