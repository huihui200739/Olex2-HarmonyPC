// Every observation here is isolated synthetic data, not a real screen measurement.
import test from 'node:test';
import assert from 'node:assert/strict';
import { validFontDisplayObservation } from '../../experiments/gx-font-display/model/FontDisplayMetrics.ts';
const normal = () => ({displayId: 0, dpiX: 144, dpiY: 192, densityPixels: 2});
test('synthetic valid primary and secondary display observations', () => {
  assert.equal(validFontDisplayObservation(normal()), true);
  assert.equal(validFontDisplayObservation({...normal(), displayId: 9007199254740991}), true);
});
for (const field of ['displayId','dpiX','dpiY','densityPixels']) {
  test('synthetic nonfinite/negative ' + field + ' is rejected', () => {
    for (const value of [NaN,Infinity,-Infinity,-1]) {
      assert.equal(validFontDisplayObservation({...normal(),[field]:value}),false);
    }
  });
}
test('zero/noninteger or unsafe display observation rejected', () => {
  for(const patch of [{displayId:.5},{displayId:9007199254740992},{dpiX:0},{dpiY:0},
    {dpiX:8193},{dpiY:8193},{densityPixels:0},{densityPixels:65},{dpiX:1,dpiY:8192}]) {
    assert.equal(validFontDisplayObservation({...normal(),...patch}),false);
  }
});
test('synthetic full bounded DPI/density grid accepted without substitution', () => {
  for(let dpi=72;dpi<=384;dpi+=3)for(const densityPixels of [0.5,1,1.5,2,3]) {
    const value={displayId:dpi,dpiX:dpi,dpiY:dpi,densityPixels};
    const before=JSON.stringify(value);assert.equal(validFontDisplayObservation(value),true);
    assert.equal(JSON.stringify(value),before);
  }
});
