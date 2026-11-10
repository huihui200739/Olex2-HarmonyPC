// All document/port fixtures here are isolated synthetic memory, never real records.
import test from 'node:test';
import assert from 'node:assert/strict';
import { OptionsDocumentTransfer, MAXIMUM_OPTIONS_DOCUMENT_BYTES, readBoundedOptionsStream,
  writeAndVerifyOptionsStream } from '../../app/entry/src/main/ets/model/OptionsDocumentTransfer.ts';
const bytes = new TextEncoder().encode('# synthetic\nui.commandHints=false\nsynthetic.label=中文\n');
const sha = 'a'.repeat(64);
function fixture() {
  const f = { snapshots: 0, merges: 0, reads: 0, writes: 0, picks: 0, uris: ['file://synthetic-fixture/settings.options'] };
  f.backend = { snapshot() { f.snapshots++; return { passed: true, data: bytes.slice(), sha256: sha, error: '' }; },
    merge(actual) { f.merges++; assert.deepEqual(actual, bytes); return { passed: true, importedOptions: 2, optionsChanged: true, sha256: sha, error: '' }; } };
  f.port = { async selectExport() { f.picks++; return f.uris; }, async selectImport() { f.picks++; return f.uris; },
    read() { f.reads++; return bytes.slice(); }, writeVerified(uri, actual) { f.writes++; assert.equal(uri, f.uris[0]); assert.deepEqual(actual, bytes); } };
  return f;
}
test('export writes verified original-format snapshot without importing', async () => {
  const f=fixture(), r=await new OptionsDocumentTransfer().run('export',f.backend,f.port);
  assert.equal(r.state,'passed');assert.equal(r.byteCount,bytes.length);assert.equal(r.sha256,sha);assert.equal(r.optionsChanged,false);
  assert.deepEqual([f.snapshots,f.picks,f.writes,f.reads,f.merges],[1,1,1,0,0]);
});
test('import reads selected bytes once and consumes original backend result', async () => {
  const f=fixture(), r=await new OptionsDocumentTransfer().run('import',f.backend,f.port);
  assert.equal(r.state,'passed');assert.equal(r.importedOptions,2);assert.equal(r.optionsChanged,true);assert.equal(r.sha256,sha);
  assert.deepEqual([f.snapshots,f.picks,f.writes,f.reads,f.merges],[0,1,0,1,1]);
});
for(const operation of ['import','export'])test('cancel '+operation+' does not touch selected files or merge options',async()=>{
  const f=fixture();f.uris=[];const r=await new OptionsDocumentTransfer().run(operation,f.backend,f.port);
  assert.equal(r.state,'cancelled');assert.equal(r.selectedUri,'');assert.equal(r.byteCount,0);assert.equal(r.optionsChanged,false);
  assert.deepEqual([f.reads,f.writes,f.merges],[0,0,0]);
});
for(const uris of [['file://one','file://two'],[''],['https://synthetic.invalid/options']])test('unsupported document selection rejected '+JSON.stringify(uris),async()=>{
  const f=fixture();f.uris=uris;const r=await new OptionsDocumentTransfer().run('import',f.backend,f.port);
  assert.equal(r.state,'failed');assert.deepEqual([f.reads,f.writes,f.merges],[0,0,0]);
});
test('export snapshot failure does not display picker',async()=>{
 const f=fixture();f.backend.snapshot=()=>({passed:false,data:new Uint8Array(),sha256:'',error:'synthetic snapshot failure'});
 const r=await new OptionsDocumentTransfer().run('export',f.backend,f.port);assert.equal(r.state,'failed');assert.equal(f.picks,0);
});
for(const size of [0,MAXIMUM_OPTIONS_DOCUMENT_BYTES+1])test('bounded import rejects byte count '+size,async()=>{
 const f=fixture();f.port.read=()=>new Uint8Array(size);const r=await new OptionsDocumentTransfer().run('import',f.backend,f.port);
 assert.equal(r.state,'failed');assert.equal(f.merges,0);
});
test('rejected original merge remains failed without successful counters',async()=>{
 const f=fixture();f.backend.merge=()=>({passed:false,optionsChanged:false,importedOptions:0,sha256:'',error:'synthetic invalid option'});
 const r=await new OptionsDocumentTransfer().run('import',f.backend,f.port);assert.equal(r.state,'failed');assert.equal(r.optionsChanged,false);assert.equal(r.importedOptions,0);
});
test('provider write failure reports possible partial destination, without deletion',async()=>{
 const f=fixture();f.port.writeVerified=()=>{throw Error('synthetic short provider write');};
 const r=await new OptionsDocumentTransfer().run('export',f.backend,f.port);assert.equal(r.state,'failed');assert.equal(r.destinationMayBePartial,true);assert.equal(r.byteCount,0);
});
test('picker failure is not misreported as cancellation',async()=>{
 const f=fixture();f.port.selectImport=async()=>{throw Error('synthetic unavailable provider');};
 const r=await new OptionsDocumentTransfer().run('import',f.backend,f.port);assert.equal(r.state,'failed');assert.equal(r.destinationMayBePartial,false);assert.equal(f.merges,0);
});
test('one active chooser at a time and later operations advance sequence',async()=>{
 const f=fixture(), transfer=new OptionsDocumentTransfer();let release;f.port.selectExport=()=>new Promise(r=>{f.picks++;release=r;});
 const first=transfer.run('export',f.backend,f.port);const overlapping=await transfer.run('import',f.backend,f.port);
 assert.equal(overlapping.sequence,1);assert.equal(overlapping.state,'choosing');assert.equal(f.picks,1);assert.equal(f.merges,0);
 release([]);assert.equal((await first).state,'cancelled');assert.equal((await transfer.run('import',f.backend,f.port)).sequence,2);
});
class SyntheticStream {
 constructor(initial=new Uint8Array()) {this.bytes=initial.slice();this.position=0;this.maximumRead=3;this.maximumWrite=2;this.flushes=0;this.seeks=0;}
 read(maximum){const n=Math.min(maximum,this.maximumRead,this.bytes.length-this.position);const value=this.bytes.slice(this.position,this.position+n);this.position+=n;return value;}
 write(value){const n=Math.min(value.length,this.maximumWrite);const size=Math.max(this.bytes.length,this.position+n), next=new Uint8Array(size);next.set(this.bytes);next.set(value.slice(0,n),this.position);this.bytes=next;this.position+=n;return n;}
 seekStart(){this.position=0;this.seeks++;}flush(){this.flushes++;}
}
test('short provider reads preserve every UTF-8 byte',()=>{assert.deepEqual(readBoundedOptionsStream(new SyntheticStream(bytes)),bytes);});
test('short provider writes are completed, flushed, sought and read back',()=>{
 const stream=new SyntheticStream();writeAndVerifyOptionsStream(stream,bytes);assert.deepEqual(stream.bytes,bytes);assert.equal(stream.flushes,1);assert.equal(stream.seeks,1);
});
for(const count of [0,-1,0.5,bytes.length+1])test('invalid provider write progress rejected '+count,()=>{
 const stream=new SyntheticStream();stream.write=()=>count;assert.throws(()=>writeAndVerifyOptionsStream(stream,bytes));assert.equal(stream.flushes,0);
});
test('exact document size bound is readable',()=>{
 const stream=new SyntheticStream(new Uint8Array(MAXIMUM_OPTIONS_DOCUMENT_BYTES));stream.maximumRead=4096;assert.equal(readBoundedOptionsStream(stream).length,MAXIMUM_OPTIONS_DOCUMENT_BYTES);
});
test('over-budget document stream rejected',()=>{
 const stream=new SyntheticStream(new Uint8Array(MAXIMUM_OPTIONS_DOCUMENT_BYTES+1));stream.maximumRead=4096;assert.throws(()=>readBoundedOptionsStream(stream));
});
test('readback byte mismatch is not a successful export',()=>{
 const stream=new SyntheticStream();stream.seekStart=()=>{stream.position=0;stream.bytes[0]^=1;};assert.throws(()=>writeAndVerifyOptionsStream(stream,bytes),/bytes differ/);
});
test('readback length mismatch is not a successful export',()=>{
 const stream=new SyntheticStream();stream.seekStart=()=>{stream.position=0;stream.bytes=stream.bytes.slice(1);};assert.throws(()=>writeAndVerifyOptionsStream(stream,bytes),/length differs/);
});
for(const method of ['flush','seekStart'])test('provider '+method+' failure propagates',()=>{
 const stream=new SyntheticStream();stream[method]=()=>{throw Error('synthetic '+method+' failure');};assert.throws(()=>writeAndVerifyOptionsStream(stream,bytes),/synthetic/);
});
