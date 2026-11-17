"""Isolated synthetic copies only; no app operations and no real record edits."""
import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from desktop_shell_validation import identity_matches,version_visible_matches,labels_match,enter_matches,identity_snapshots,IDENTITY_SOURCE
from application_runtime_validation import SOURCE as APP_SOURCE
from desktop_command_validation import SOURCE as VIEW_SOURCE
ROOT=Path(__file__).resolve().parents[2]

class DesktopShellValidationTests(unittest.TestCase):
    def setUp(self):
        self.expected=dict(bundleName='isolated.synthetic.example',versionName='test-version',versionCode=210000)
        self.identity=dict(self.expected,error='',source=IDENTITY_SOURCE)
        self.labels=dict(entry=[0,0,192,160],view=dict(text='执行视图',visible='true',bounds=[10,46,93,76]),application=dict(text='应用函数',visible='true',bounds=[99,46,182,76]),help=dict(text='命令帮助',visible='true',bounds=[10,82,182,110]))

    def test_synthetic_identity_exact_and_no_fallback(self):
        self.assertTrue(identity_matches(self.identity,self.expected))
        for key,value in [('bundleName','other'),('versionName','0.14.0'),('versionCode',True),('versionCode',210001),('error','read failed'),('source','source-json')]:
            with self.subTest(key=key,value=value):
                changed=copy.deepcopy(self.identity);changed[key]=value
                self.assertFalse(identity_matches(changed,self.expected))

    def test_synthetic_visible_version_must_match_installed(self):
        sample=dict(identity=self.identity,visible_text='开发验证版  /  test-version')
        self.assertTrue(version_visible_matches(sample,self.expected))
        for text in ['开发验证版  /  0.14.0','test-version','开发验证版  /  ']:
            sample['visible_text']=text;self.assertFalse(version_visible_matches(sample,self.expected))

    def test_synthetic_labels_use_two_rows_and_real_bounds(self):
        self.assertTrue(labels_match(self.labels))
        for field,value in [('text','执…'),('visible','false'),('bounds',[10,46,59,76]),('bounds',[-1,46,93,76])]:
            changed=copy.deepcopy(self.labels);changed['view'][field]=value;self.assertFalse(labels_match(changed))
        changed=copy.deepcopy(self.labels);changed['help']['bounds']=[140,46,182,76];self.assertFalse(labels_match(changed))

    def sample(self,route):
        retained=ROOT/'reports/app/0.20.0-pre-desktop-shell-summaries/reports__app__last-run.json'
        actual=json.loads(retained.read_text())['startup_report']
        keys=('applicationRuntime','applicationFunction','desktopCommand')
        before={k:copy.deepcopy(actual[k]) for k in keys}
        after=copy.deepcopy(before)
        if route=='application':
            after['applicationFunction']=dict(sequence=before['applicationFunction']['sequence']+1,input='app.ConfigDir()',passed=True,optionsChanged=False,text=before['applicationRuntime']['configDir'][:-1],error='',source=APP_SOURCE)
        else:
            after['desktopCommand']=dict(sequence=before['desktopCommand']['sequence']+1,input='pc.help',passed=True,changed=False,applied=False,dispatched=1,message='pc.mesh sphere|torus synthetic metadata. New platform view adapters only, not the complete Olex2 command library.',error='',source=VIEW_SOURCE)
        return dict(before=before,after=after,observed_input='app.ConfigDir()' if route=='application' else 'pc.help',injected_keycode=2054)

    def test_synthetic_enter_application_preserves_original_text_and_other_interpreter(self):
        sample=self.sample('application');self.assertTrue(enter_matches(sample,'application'))
        for key,value in [('sequence',1000),('text','invented'),('input','app.BaseDir()'),('source','replacement'),('optionsChanged',True),('passed',False)]:
            changed=copy.deepcopy(sample);changed['after']['applicationFunction'][key]=value;self.assertFalse(enter_matches(changed,'application'))
        changed=copy.deepcopy(sample);changed['after']['applicationFunction']['text']=changed['after']['applicationRuntime']['configDir'];self.assertFalse(enter_matches(changed,'application'))
        sample['after']['desktopCommand']['sequence']+=1;self.assertFalse(enter_matches(sample,'application'))

    def test_synthetic_enter_view_does_not_call_application(self):
        sample=self.sample('view');self.assertTrue(enter_matches(sample,'view'))
        for key,value in [('changed',True),('applied',True),('dispatched',0),('source','replacement'),('message',''),('error','failed')]:
            changed=copy.deepcopy(sample);changed['after']['desktopCommand'][key]=value;self.assertFalse(enter_matches(changed,'view'))
        sample['after']['applicationFunction']['sequence']+=1;self.assertFalse(enter_matches(sample,'view'))

    def test_synthetic_injected_key_and_observed_text_required(self):
        sample=self.sample('application')
        sample['injected_keycode']=2017;self.assertFalse(enter_matches(sample,'application'))
        sample=self.sample('application');sample['observed_input']='app.ConfigDir';self.assertFalse(enter_matches(sample,'application'))

    def test_synthetic_identity_walk_does_not_ignore_missing_runtime_identity(self):
        self.assertEqual(list(identity_snapshots(dict(applicationRuntime={}))),[None])
        self.assertEqual(list(identity_snapshots([dict(applicationRuntime={},installedPackage=self.identity)])),[self.identity])

    def test_synthetic_missing_fields_rejected(self):
        for value in [None,{},[],True,'text']:
            self.assertFalse(identity_matches(value,self.expected));self.assertFalse(labels_match(value));self.assertFalse(enter_matches(value,'application'))

if __name__=='__main__':unittest.main()
