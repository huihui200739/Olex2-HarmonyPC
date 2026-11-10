// All validation fixtures are isolated synthetic preference objects, not real records.
import test from 'node:test';
import assert from 'node:assert/strict';
import { defaultDesktopState, parseDesktopState } from '../../app/entry/src/main/ets/model/DesktopState.ts';

const encode = (patch = {}) => JSON.stringify({ ...defaultDesktopState(), ...patch });
test('default preference round trip', () => {
  assert.deepEqual(parseDesktopState(encode()), defaultDesktopState());
});
test('all helper/detail/mode preference combinations round trip', () => {
  for (let primitive = 0; primitive < 4; primitive++) {
    for (let generation = 0; generation < 6; generation++) {
      for (const wireframe of [false, true]) {
        const value = { ...defaultDesktopState(), primitive, generation, wireframe,
          yaw: -359.5, pitch: 71, distance: 7.25 };
        assert.deepEqual(parseDesktopState(JSON.stringify(value)), value);
      }
    }
  }
});
for (const [name, patch] of Object.entries({
  'unknown schema': { schemaVersion: 5 }, 'string schema': { schemaVersion: '1' },
  'negative primitive': { primitive: -1 }, 'unknown primitive': { primitive: 4 },
  'fractional primitive': { primitive: 0.5 }, 'negative detail': { generation: -1 },
  'unknown detail': { generation: 6 }, 'fractional detail': { generation: 1.5 },
  'string boolean': { wireframe: 'true' }, 'numeric boolean': { wireframe: 1 },
  'null angle': { yaw: null }, 'string angle': { yaw: '32' },
  'out of range yaw': { yaw: 361 }, 'out of range pitch': { pitch: 86 },
  'below minimum zoom': { distance: 2 }, 'above maximum zoom': { distance: 11 },
})) {
  test('synthetic invalid preference rejected: ' + name, () => {
    assert.equal(parseDesktopState(encode(patch)), undefined);
  });
}
test('missing fields rejected', () => {
  for (const field of Object.keys(defaultDesktopState())) {
    const value = { ...defaultDesktopState() };
    delete value[field];
    assert.equal(parseDesktopState(JSON.stringify(value)), undefined);
  }
});
test('synthetic invalid JSON / root types rejected', () => {
  for (const value of ['', '{', 'null', '[]', '1', 'true', '"text"']) {
    assert.equal(parseDesktopState(value), undefined);
  }
});
test('non-finite angle rejected', () => {
  assert.equal(parseDesktopState(encode().replace('"yaw":32', '"yaw":1e999')), undefined);
});
test('explicit preference size boundary enforced', () => {
  assert.equal(parseDesktopState(' '.repeat(4097)), undefined);
});
test('unknown metadata is not carried into saved state', () => {
  assert.deepEqual(parseDesktopState(encode({ syntheticExtraMetadata: { ignored: true } })), defaultDesktopState());
});
test('numeric limits remain supported', () => {
  for (const patch of [{ yaw: -360, pitch: -85, distance: 2.2 }, { yaw: 360, pitch: 85, distance: 10 }]) {
    assert.deepEqual(parseDesktopState(encode(patch)), { ...defaultDesktopState(), ...patch });
  }
});

const { DesktopStateSaveQueue } = await import('../../app/entry/src/main/ets/model/DesktopState.ts');
test('queue saves first state once', () => {
  const queue = new DesktopStateSaveQueue();
  const state = defaultDesktopState();
  assert.equal(queue.stage(state), 'changed');
  assert.deepEqual(queue.pendingState(), state);
  queue.complete(true);
  assert.equal(queue.stage(state), 'saved');
  assert.equal(queue.pendingState(), undefined);
});
test('queue coalesces unchanged polling', () => {
  const queue = new DesktopStateSaveQueue();
  queue.stage(defaultDesktopState());
  assert.equal(queue.stage(defaultDesktopState()), 'pending');
});
test('rapid return to saved state cancels stale pending write', () => {
  const queue = new DesktopStateSaveQueue();
  const original = defaultDesktopState();
  queue.stage(original); queue.complete(true);
  assert.equal(queue.stage({ ...original, yaw: 42 }), 'changed');
  assert.equal(queue.stage(original), 'saved');
  assert.equal(queue.pendingState(), undefined);
});
test('failure is retained without repeated polling writes', () => {
  const queue = new DesktopStateSaveQueue();
  const state = defaultDesktopState();
  queue.stage(state); queue.complete(false);
  assert.equal(queue.stage(state), 'write-error');
  assert.equal(queue.pendingState(), undefined);
});
test('a changed view can save after an earlier failure', () => {
  const queue = new DesktopStateSaveQueue();
  const state = defaultDesktopState();
  queue.stage(state); queue.complete(false);
  const next = { ...state, yaw: 42 };
  assert.equal(queue.stage(next), 'changed');
  queue.complete(true);
  assert.equal(queue.stage(next), 'saved');
});
test('latest coalesced snapshot wins', () => {
  const queue = new DesktopStateSaveQueue();
  const state = defaultDesktopState();
  queue.stage(state);
  queue.stage({ ...state, generation: 2 });
  const next = { ...state, generation: 5, primitive: 3 };
  queue.stage(next);
  assert.deepEqual(queue.pendingState(), next);
});

test('schema1 migrates view settings without losing selection', () => {
  const legacy = { schemaVersion: 1, primitive: 2, generation: 5, wireframe: true, yaw: 42, pitch: -32, distance: 6.2 };
  assert.deepEqual(parseDesktopState(JSON.stringify(legacy)), { ...legacy, schemaVersion: 4, colorPreset: 0, materialPreset: 0 });
});
test('schema1 does not treat unknown color metadata as schema2', () => {
  assert.equal(parseDesktopState(encode({ schemaVersion: 1, colorPreset: 999 })).colorPreset, 0);
});
test('all three color presets round trip', () => {
  for (let colorPreset = 0; colorPreset < 3; colorPreset++) {
    assert.deepEqual(parseDesktopState(encode({ colorPreset })), { ...defaultDesktopState(), colorPreset });
  }
});
for (const colorPreset of [-1, 3, 0.5, '1', null]) {
  test('invalid schema2 color preset rejected: ' + JSON.stringify(colorPreset), () => {
    assert.equal(parseDesktopState(encode({ colorPreset })), undefined);
  });
}

test('schema2 migrates previous color and view with default material', () => {
  const legacy = { ...defaultDesktopState(), schemaVersion: 2, colorPreset: 2, yaw: 52 };
  delete legacy.materialPreset;
  assert.deepEqual(parseDesktopState(JSON.stringify(legacy)), { ...legacy, schemaVersion: 4, materialPreset: 0 });
});
test('all supported selection, color and material combinations round trip', () => {
  for (let primitive=0; primitive<4; primitive++) for (let generation=0; generation<6; generation++) {
    for (const wireframe of [false,true]) for (let colorPreset=0; colorPreset<3; colorPreset++) {
      for (let materialPreset=0; materialPreset<4; materialPreset++) {
        const state={ ...defaultDesktopState(),primitive,generation,wireframe,colorPreset,materialPreset };
        assert.deepEqual(parseDesktopState(JSON.stringify(state)),state);
      }
    }
  }
});
for (const materialPreset of [-1,4,.5,'1',null]) {
  test('invalid schema4 material rejected: '+JSON.stringify(materialPreset),()=> {
    assert.equal(parseDesktopState(encode({ materialPreset })),undefined);
  });
}
test('legacy material metadata does not override migration default',()=> {
  assert.equal(parseDesktopState(encode({ schemaVersion: 2,materialPreset: 999 })).materialPreset,0);
});

test('schema3 retains its original material range and migrates to schema4',()=> {
  for(let materialPreset=0;materialPreset<3;materialPreset++){
    const value={...defaultDesktopState(),schemaVersion:3,materialPreset,colorPreset:1};
    assert.deepEqual(parseDesktopState(JSON.stringify(value)),{...value,schemaVersion:4});
  }
  assert.equal(parseDesktopState(encode({schemaVersion:3,materialPreset:3})),undefined);
});
test('glossy material survives schema4 preference round trip',()=> {
  assert.deepEqual(parseDesktopState(encode({materialPreset:3})),{...defaultDesktopState(),materialPreset:3});
});
