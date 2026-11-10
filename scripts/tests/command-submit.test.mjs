// Isolated synthetic expression strings; no original commands execute here.
import test from 'node:test';
import assert from 'node:assert/strict';
import { usesApplicationInterpreter } from '../../app/entry/src/main/ets/model/CommandSubmit.ts';
for (const input of ['app.ConfigDir()', 'app.ArgCount()', 'app.SetOption(ui.commandHints,true)', '  app.BaseDir()', '\tapp.Platform()', '\napp.SharedDir()', 'app.Missing()', 'app.', 'app.ConfigDir()  ']) {
  test('restricted application namespace routes without changing input: ' + JSON.stringify(input), () => {
    const before = input;
    assert.equal(usesApplicationInterpreter(input), true);
    assert.equal(input, before);
  });
}
for (const input of ['pc.help', 'pc.mesh torus', ' pc.help', '', ' ', 'App.ConfigDir()', 'application.ConfigDir()', 'xapp.ConfigDir()', 'core.LstMac', 'shell.echo']) {
  test('other input remains on existing restricted view interpreter: ' + JSON.stringify(input), () => {
    assert.equal(usesApplicationInterpreter(input), false);
  });
}
